all: service device

service: service.cc
	clang++ -I. -O2 -g -fsanitize=address,undefined -Wall -std=c++1y -lpaho-mqtt3c -lpaho-mqttpp3 -lpaho-mqtt3a service.cc -o service

device: device.cc
	clang++ -I. -O2 -g -fsanitize=address,undefined -Wall -std=c++1y -lpaho-mqtt3c -lpaho-mqttpp3 -lpaho-mqtt3a device.cc -o device

clean:
	rm -f service device
