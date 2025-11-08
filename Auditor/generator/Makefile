.PHONY: build freeze

build:
	$(MAKE) build-py
	$(MAKE) build-go

build-py: main.py json_generator.py sender.py
	pip install pipreqs
	pip install -r requirements.txt

build-go: main.go go.mod
	go build -C . -o testserver
	
freeze:
	pipreqs . --force