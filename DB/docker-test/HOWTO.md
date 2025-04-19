# Пример кластера go-сервисов 

### 1. Создаем вспомогательный образ

На него установим необходимые инструменты и в дальнейшем будем из него собирать образ с сервером. (Все команды запускаются из корневой директории проекта)

```bash
sudo docker build --network host -f docker-test/Dockerfile2 -t fedora-go .
```
### 2. Проверяем что компилируются все исполняемые файлы

В данном примере один исполняемый файл -- бинарь сервера:

```bash
  go build -C ./cmd -o $(pwd)/service/server
```

В общем случае сборка go проекта:

```bash
  go build -C ./cmd_srv -o /path/to/exec_dir/server
  go build -C ./cmd_cl  -o /path/to/exec_dir/client
  ...
```

Флаги санитайзеров: `-race`, `-msan`, `-asan`

### 3. Деплой кластера

Разворачиваем кластер

```bash
sudo ./run_tests.sh
```
Нормальное выключение кластера: 1 раз `Ctrl+C` \
Форсированное: 2 раза `Ctrl+C`

В обоих случаях структурированные логи каждого контейнера появятся в папке [docker-test](.)

### 4. Структура кластера

Структура описана в [docker-compose.yml](./docker-compose.yml):

Создаем ноды из шаблона:  
```yml
x-service-base: &base
  privileged: true
  image: my-db
  user: root
  environment:
    - CFG_PATH=/root/Cluster.config
  networks:
    - netDB
```
`my-db` создается сразу перед деплоем кластера в скрипте [run_tests.sh](../run_tests.sh) комнадой 
```bash
docker build --network host --no-cache -f docker-test/Dockerfile -t my-db .
```

Нода устроена следующим образом: (+ наследование кода шаблона `x-service-base`)
```yml
node1:
  <<: *base
  container_name: ci_node1
  hostname: node1
  command: |
    /bin/bash -c "
      set -ex

      ./setup.sh node1

      sleep inf
    "
```
`hostname`: имя контейнера в сети. все контейнеры в сети с текущим будут успешно выполнять `ping 'hostname'`
`command`: стартуем сервер и спим. `./setup.sh node1` Запишет ip адреса всех других нод в сети в файл `/root/Cluster.config`, а свой в переменную окружения `SELF_IP`. Конфиг скармливаем серверу, теперь он знает и свой адрес и таблицу адресов реплик.


Сеть `netDB` видна только хосту, можно делать запросы к кластеру, но при этом в большинстве случаях хостовый днс не резолвит `hostname` контейнеров в сети, так что нужно каждый раз резолвить их адреса, что не удобно, так что есть два пути тестирования и работы с кластером, о которых ниже

Из ноды `ci_tester` можно обращаться к серверу ноде `i` по адресу `http://node%i:40404/endpoint` -- там днс резолвит

Выставляем порт наружу кластера. Теперь можно обращаться к серверу на `node1` с хоста по запросу `http://127.0.0.1:40401/endpoint`

```yml
node1:
  <<: *base
  container_name: ci_node1
  hostname: node1
  ports:
    - "40404:40401"
  command: |
    /bin/bash -c "
      set -ex

      ./setup.sh node1

      sleep inf
    "
```

Можем проделать так же для каждой ноды:
```yml
node2:
  ports:
    - "40404:40402"

node3:
  ports:
    - "40404:40403"

node4:
  ports:
    - "40404:40404"

node5:
  ports:
    - "40404:40405"
```

Теперь можем с хоста обращаться к серверу на ноде `i`: `http://127.0.0.1:4040%i/endpoint = http://node%i:40404/endpoint`
