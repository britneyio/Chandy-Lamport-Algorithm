FROM ubuntu:latest

RUN apt-get update && apt-get install -y gcc

ADD main.c /prj2/
ADD hostsfile.txt /prj2/
WORKDIR /prj2/ 
RUN gcc main.c -o main 

ENTRYPOINT ["/prj2/main"]