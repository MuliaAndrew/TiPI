FROM fedora-go:latest

WORKDIR /DB
COPY . .

RUN go build -C ./cmd/server -a -asan -o /root/DBserver

RUN cp -r ./tools/* /root/

WORKDIR /root