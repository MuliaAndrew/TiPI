FROM fedora-go:latest

WORKDIR /DB
COPY . .

RUN go build -C ./cmd -o /root/DBserver

RUN cp -r ./service/* /root/

COPY ./service/server.service /etc/systemd/system/DB.service

WORKDIR /root