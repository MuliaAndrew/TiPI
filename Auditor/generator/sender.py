import time
import asyncio
import signal
import threading
from urllib3.util import Url
from typing import Any, Dict, Self
import json_generator
import logging
from logging import INFO, DEBUG, CRITICAL, FATAL, WARNING
import aiohttp

log = logging.getLogger().getChild('engine')
sender = None

async def shutdown(sig, loop: asyncio.AbstractEventLoop):
  """
  Cancels all running non-main tasks and waits for them to complete.
  """
  # Using log.log(logging.INFO, ...) or the cleaner log.info(...)
  log.info(f"Received exit signal {sig}...")
  
  # 1. Get all tasks currently running on the loop
  tasks = [t for t in asyncio.all_tasks(loop=loop) if t is not asyncio.current_task()]
  
  # 2. Cancel all non-main tasks
  log.info(f"Cancelling {len(tasks)} outstanding tasks...")
  for task in tasks:
    task.cancel()

  await asyncio.wait()
  
  # 4. Stop the event loop
  loop.stop()

def setup_signal_handler(loop: asyncio.AbstractEventLoop):
  """Registers the asynchronous shutdown function."""
  for sig in (signal.SIGINT, signal.SIGTERM):
    loop.add_signal_handler(
      sig, 
      lambda: asyncio.create_task(shutdown(sig, loop))
    )

#---------------------------------------------------------------------------------------------------------

class Stats():
  def __init__(self):
    super().__init__()
    self.errors = dict() # map [HttpErrorCode 400-599, count]
    self.reqs_count = 0 # total requests have been sended
    self.dead_network_connections = 0
    self.reqs_complited = 0  # total requests have been responsed with status < 400
    self.mtx = threading.Lock()
  
  def acquire(self):
    return self.mtx.acquire()
  
  def release(self):
    return self.mtx.release()
  
  def locked(self):
    return self.mtx.locked()

  def __enter__(self):
    self.acquire()
    return self
  

  def __exit__(self, exc_type, exc_val, exc_tb):
    self.release()
    return False

  def time_init(self) -> None:
    '''
    Factoried out from __init__ since no need to present in every stat object, only in top one
    '''
    self.ts_start = time.time_ns() # can be accessed without lock since it readonly

  #raise error if called before time_init
  def time_spent(self) -> float:
    return float(time.time_ns() - self.ts_start)
  
  def merge(self, st: Self):
    '''
    Merge st into this Stats object. Thread safe
    '''
    with st and self:
      self.errors.update(st.errors)
      self.reqs_count += st.reqs_count
      self.reqs_complited += st.reqs_complited
      self.dead_network_connections += st.dead_network_connections

#---------------------------------------------------------------------------------------------------------

class SenderBase:
  '''
  Sender is interface for senders (http, directory)
  '''
  def __init__(self, json_shema: Dict[str, Any], nclients: int = 1, freq: float = 0.0, bad_req_rate: float = 0.0, bad_json_rate: float = 0.0):
    global sender
    if sender is not None:
      raise TypeError().with_traceback().add_note('SenderBase is singleton')
    self.stats = Stats()
    self.freq = freq
    self.bad_req_rate = bad_req_rate
    self.bad_json_rate = bad_json_rate
    self.generator = json_generator.create_json_generator_from_schema(json_shema)
    self.nclients = nclients
    sender = self
  
  def __repr__(self):
    with self.stats:
      res = f"    --- Perfomance statistics ---    \n" \
            f"Time spent: {self.stats.time_spent() / 10**9:.03f} s\n" \
            f"------------------------------------\n" \
            f"Requests sended: {self.stats.reqs_count}\n" \
            f"Request complited successfully: {self.stats.reqs_complited}\n" \
            f"Request completition rate: {float(self.stats.reqs_complited) / self.stats.reqs_count * 100:.3}%\n" \
            f"Bad requests count: \n" \
            f"Dead connections count: \n" \
            f"------------------------------------\n" \
            f"Average request latency: \n" \
            f"Average bandwidth:  MB\n" \
            f"Average RPS (Total): {self.stats.reqs_count / (self.stats.time_spent() / 10**9):.03f} \n" \
            f"Average RPS (For single http client): {self.stats.reqs_count / (self.stats.time_spent() / 10**9) / self.nclients:.03f}\n" \
            f"------------------------------------\n"
    return res

#---------------------------------------------------------------------------------------------------------

class HttpSender(SenderBase):
  '''
  Implements http sender that continiously sends requests to auditor and collect perfomance statistics
  '''

  def __init__(self,
               schema: Dict[str, Any],
               url: Url, 
               num_http_clients: int = 1, 
               freq: float = 0.0, 
               bad_req_rate: float = 0.0, 
               bad_json_rate: float = 0.0):
    super().__init__(schema, num_http_clients, freq, bad_req_rate, bad_json_rate)
    self.stats.time_init()
    self.url = url
    self.nclients = num_http_clients
    log.log(INFO, f'init HttpSender %p = {id(self)} with {self.nclients} http clients total')

  async def _http_client_run(self, cl_id, _log: logging.Logger):
    '''
    Run single http client.
    Main logic here
    '''
    _log.log(INFO, 'Enter')
    st = Stats()
    while True:
      try:
        async with aiohttp.ClientSession() as cl:
          while True:
            st.reqs_count += 1
            try:
              async with cl.post(
                str(self.url),
                json=self.generator.generate()
              ) as resp:
                if resp.status < 400:
                  st.reqs_complited += 1
                else:
                  resp.raise_for_status()
            except asyncio.CancelledError or KeyboardInterrupt:
              raise asyncio.CancelledError
            except aiohttp.ClientResponseError as e:
              _log.log(WARNING, f'Response error: {e.status} - {e.message}')
              if e.status in st.errors:
                st.errors[e.status] += 1
              else:
                st.errors[e.status] = 1
              raise ConnectionError
            except aiohttp.ClientConnectionError:
              _log.log(WARNING, 'Client connection error')
              raise ConnectionError
            except aiohttp.ServerTimeoutError or aiohttp.SocketTimeoutError:
              _log.log(WARNING, 'Timed out')
              raise ConnectionError
            except aiohttp.ClientError as e:
              # Catch general aiohttp exception
              _log.log(WARNING, f'Catched {e}')
              break
            except aiohttp.ClientTimeout:
              raise TimeoutError

      except TimeoutError:
        # Assume server is busy, sleep a little before reset
        try:
          await asyncio.sleep(0.01)
        except asyncio.CancelledError:
          break
        pass
      except ConnectionError:
        # Ok we was reseted or smth. Ignore and try to reconnect. sleep a little before reset
        try:
          await asyncio.sleep(0.01)
        except asyncio.CancelledError:
          break
        pass
      except asyncio.CancelledError:
        break
      finally:
        st.dead_network_connections += 1

    
    _log.log(INFO, 'Cancelled')
    self.stats.merge(st)

  async def _main(self):
    '''
    Root coroutine to spawn and control http clients
    '''
    loop = asyncio.get_event_loop()
    setup_signal_handler(loop)

    tasks = []
    for i in range(self.nclients):
      tasks.append(self._http_client_run(i, log.getChild(f'cl#{i}')))
    
    try:
      L = await asyncio.gather(*tasks)
    except asyncio.CancelledError:
      print(self)
  
  def run(self):
    '''
    Main loop. Init and run http client(s) senders with data from json generator
    '''

    try:
      asyncio.run(self._main())
    # except Exception as e:
    #   print(e)
    finally:
      log.log(INFO, 'Done')


class FileSender(SenderBase):
  '''
  Implements file sender that continiously write constructed requests to files
  '''
  pass