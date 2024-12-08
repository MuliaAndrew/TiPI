#!/usr/bin/python3

import numpy as np
import matplotlib.pyplot as plt
from typing import List

NUM_THREADS = 32

def process_lock_stat(lockname: str, average: list, maximum: list):
  if lockname == "TICK": lockname = "Ticket"
  lockname += "Lock время вхождения"
  
  x = np.arange(1, NUM_THREADS+1)
  y1_average = np.abs(np.array(average))
  y1_maximum = np.array(list(int(elem // 1000) for elem in maximum))

  plt.plot(x, y1_average, '.r', linestyle='', label="Среднее за 50000 итераций")
  plt.plot(x, y1_maximum, '.g', linestyle='', label="Максимальное за 50000 итераций")
  plt.xlabel("$n$ потоков")
  plt.ylabel("$\\langle t\\rangle$, нс/ $t_{max}$, мкс")
  plt.grid()
  plt.title(lockname)
  plt.legend()
  plt.tight_layout()

if __name__ == "__main__":

  params: List[str] = []
  while True:
    try:
      params.append(input())
    except:
      break
  
  n_locks = len(params) // 4

  plt.figure(figsize=[6, n_locks * 6])
  plt.subplot(n_locks, 1, 1)
  for i in range(n_locks):
    plt.subplot(n_locks, 1, i+1)
    print(params[i * 4])
    process_lock_stat(
      params[i * 4].upper(), 
      list(int(elem) for elem in params[i * 4 + 1].split()),
      list(int(elem) for elem in params[i * 4 + 2].split())
    )
  plt.savefig(fname="result.pdf")