CONTAINERS=(ci_node1 ci_node2 ci_node3 ci_node4 ci_node5)

docker build --network host -f docker-test/Dockerfile -t my-db . || exit $?
docker-compose -f docker-test/docker-compose.yml up --force-recreate --remove-orphans -d|| exit $?

docker wait ci_tester || echo $?

docker logs ci_tester &> docker-test/ci_tester.log || true

for ct in "${CONTAINERS[@]}"; do 
  docker stop $ct
  docker logs $ct &> docker-test/${ct}.log || true
  docker cp $ct:/var/log/DB.log docker-test/${ct}_DB.log || true
done

