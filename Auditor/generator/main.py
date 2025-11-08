#!/usr/bin/python3

import argparse
from pathlib import Path
from urllib3.util import parse_url
import sender
import json_generator
import logging

def __init__():
  logging.basicConfig(
    format="%(asctime)s:%(msecs)03d [%(levelname)s] %(name)s: %(message)s",
    filename='engine.log', filemode='a', 
    level=logging.FATAL, 
    datefmt='%Y-%m-%d  %H:%M:%S',
  )

'''
    "Tool to generate json objects for provided template and send data to file or custom http endpoint\n"
    "\n"
    "Required options:\n"
    "    -t, --template FILE       path to file with json template\n"
    "Destionation options:\n"
    "    -e, --endpoint-url URL    endpoint where to send generated data\n"
    "    -j, --thread-number N     number threads to use\n"
    "    -d, --dir DIR             store json data samples in the directory DIR\n"
    "  One of the --endpoint-url and --dir must be specified\n"
    "\n"
    "Generator options\n:"
    "    -f, --frequency F         float number, frequency of samples sending to dst. if F is 0 then unlimited\n"
    "    -P, --proto-fault-rate R  float number in [1.0, 100.0], the percentage of wrongly constructed requests\n"
    "    -R, --json-fault-rate R   float number in [1.0, 100.0], the percentage of wrongly constructed json objects\n"
    "    -r, --repeat-rate R       float number in [1.0, 100.0], the percentage of repeated requests\n"
    "    -T TIME                   run this program for TIME seconds long.
    "\n"
    "    -h, --help                print this help message\n"
'''

def InitPraser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(
    "./main.py",
    description="Tool to generate json objects for provided template and send data to file or custom http endpoint. Can be terminated and print statistics with SIGINT",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog='One and only one of the --dir or --endpoint-url must be used'
  )

  required_gr = parser.add_argument_group("Required options")
  required_gr.add_argument(
    '-t', '--template',
    required=True,
    type=Path,
    metavar='PATH',
    help="path to file with json template"
  )

  dst_gr = parser.add_argument_group("Destination options")
  dst_gr.add_argument(
    '-e', '--endpoint-url',
    required=False,
    type=parse_url,
    metavar='URL',
    help='endpoint where to send generated data'
  )

  dst_gr.add_argument(
    '-n', '--clients-number',
    required=False,
    type=int,
    default=1,
    metavar="NUM",
    help='number http connections to use'
  )

  dst_gr.add_argument(
    '-d', '--dir',
    required=False,
    type=Path,
    metavar='PATH',
    help='store json data samples in the directory DIR (NOT IMPLEMENTED)'
  )

  gen_gr = parser.add_argument_group("Generator arguments")
  gen_gr.add_argument(
    '-f', '--frequency',
    required=False,
    type=float,
    default=0, 
    metavar='FREQ',
    help='frequency of generator and http-sender in Hz for each thread. for FREQ==0 frequency is unlimited (NOT IMPLEMENTED)'
  )

  gen_gr.add_argument(
    '-P', '--proto-fault-rate',
    required=False,
    type=float,
    default=0.0,
    metavar='RATE',
    help='percentage of bad constructed POST requests. Must be a float in [0, 100] (NOT IMPLEMENTED)'
  )

  gen_gr.add_argument(
    '-R', '--json-fault-rate',
    required=False,
    type=float,
    default=0.0,
    metavar='RATE',
    help='percentage of bad constructed JSON objects for POST requests. Must be a float in [0, 100] (NOT IMPLEMENTED)'
  )

  gen_gr.add_argument(
    '-T', '--timeout',
    required=False,
    type=float,
    default=0.0,
    metavar='TOUT',
    help='after this timeout program will be stopped and print statistics'
  )

  return parser

if __name__ == '__main__':
  parser = InitPraser()
  args = parser.parse_args()
  
  if getattr(args, 'dir') is None and getattr(args, 'endpoint_url') is None:
    print("Only one of the --dir or --endpoint-url must be provided\n")
    parser.print_help()

  if getattr(args, 'dir') is not None and getattr(args, 'endpoint_url') is not None:
    print("One of the --dir or --endpoint-url must be provided\n")
    parser.print_help()
  
  __init__()

  num_clnt = args.clients_number if hasattr(args, 'clients_number') else 1

  if hasattr(args, 'endpoint_url'):
    engine = sender.HttpSender(json_generator.extract_schema_from_json_file(args.template), args.endpoint_url, num_clnt)
  
  engine.run()

  logging.getLogger().log(logging.INFO, 'Terminate\n\n')