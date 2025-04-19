CONTAINERS=(ci_node1 ci_node2 ci_node3 ci_node4 ci_node5)

do_exit() {
  docker logs ci_tester &> docker-test/ci_tester.log || true

  docker stop "${CONTAINERS[@]}"
  for ct in "${CONTAINERS[@]}"; do 
    docker logs $ct &> docker-test/${ct}.log || true
    docker cp $ct:/var/log/DB.log docker-test/${ct}_DB.log || true
  done
  docker rm "${CONTAINERS[@]}"
}

trap do_exit EXIT

docker build --network host --no-cache -f docker-test/Dockerfile -t my-db . || exit $?
docker-compose -f docker-test/docker-compose.yml up --force-recreate --remove-orphans || exit $?

echo "====== Network ========"
echo "Main bus:"
docker network inspect docker-test_netDB --format="{{range .IPAM.Config}}{{println .Subnet}}{{end}}"
echo "Container ip:"
docker network inspect docker-test_netDB --format="{{range .Containers}}{{println .Name .IPv4Address}}{{end}}" \
  | grep -v ci_tester
echo "======================="

docker wait ci_tester || echo $?
