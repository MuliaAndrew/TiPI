# must be built from deploy dir
ARG IMAGE
FROM $IMAGE

USER root

RUN apt-get update
RUN apt-get install -y curl

USER 1001:0