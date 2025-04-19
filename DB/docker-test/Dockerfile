FROM fedora-go:latest

WORKDIR /DB
COPY . .

RUN go build -C ./cmd -a -o /root/DBserver

RUN cp -r ./service/* /root/

WORKDIR /root