FROM ubuntu:22.04

RUN apt clean -y && apt update -y && apt install -y ca-certificates
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC
RUN apt-get install -y tzdata
RUN apt-get update --fix-missing

RUN apt-get install -y wget cmake g++ git less vim build-essential protobuf-compiler libprotobuf-dev

