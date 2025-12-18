# Docker image for auditord service
# must be build from project root
# uses image built with auditor_base.Dockerfile

from auditor_base:latest

COPY . .

WORKDIR auditord
RUN pwd
USER root
RUN make
USER auditor:users

WORKDIR ../deploy/

WORKDIR /home/auditor