.PHONY: build install

all: build install

build: cmd/main.go go.mod
	go build -C ./cmd -o ../auditord

# for docker image only, do not use on host
install: ./auditord ./auditor_config.yaml
	install -g users -D -m 755 ./auditord /usr/bin
	install -g users -D -m 644 ./auditor_config.yaml /etc/auditor/config.yaml
