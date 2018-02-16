FROM ubuntu:16.04 

RUN git clone https://github.com/akshshar/openr.git /root/openr && cd /root/openr/ && git checkout linux && cd /root/openr/build && ./build_openr_dependencies.sh

RUN cd /root/openr/build && ./build_openr.sh && ./remake_glog.sh && cd /root/ && rm -r /root/openr

COPY run_openr.sh /usr/sbin/run_openr.sh

CMD /usr/sbin/run_openr.sh >/var/log/openr.log 2>&1

