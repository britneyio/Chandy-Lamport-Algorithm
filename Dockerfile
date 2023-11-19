FROM ubuntu:latest

RUN apt-get update && apt-get install -y gcc

ADD newmain.c /prj2/
ADD hostsfile.txt /prj2/
WORKDIR /prj2/ 
RUN gcc newmain.c -o main 

ENTRYPOINT ["/prj2/main"]