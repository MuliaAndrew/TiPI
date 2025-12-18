# Docker base image for auditord service
# must be build from project root

FROM fedora:42

user root

RUN dnf update -y
RUN dnf install -y golang curl etcd postgresql bind-utils ping

# implicitly creates /home/auditor
RUN useradd -m -g users auditor
user auditor:users
WORKDIR /home/auditor
