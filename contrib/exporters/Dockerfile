#Image layers are not split because of pycurl C dependency issues 
FROM alpine:3 as base
#Used to allow output for debugging
ENV PYTHONUNBUFFERED=1
WORKDIR /app
RUN apk add -u --no-cache gcc g++ python3-dev \
            curl-dev py3-wheel py3-pip
ADD . .
RUN pip install --no-cache-dir -r requirements.txt
RUN chmod +x ecc_zcashd_exporter.py
CMD [ "/app/ecc_zcashd_exporter.py" ]


##Env that might simplify debugging +- bash or python env/deploy
#FROM python:3.9-slim
#ADD . /app/
#WORKDIR /app/
#RUN apt-get update && apt-get install gcc libcurl4-openssl-dev libssl-dev -y
#RUN pip install --no-cache-dir -r requirements.txt
#RUN chmod +x ecc_zcashd_exporter.py & cp ecc_zcashd_exporter.py /usr/local/bin/
#ENTRYPOINT ["/usr/local/bin/ecc_zcashd_exporter.py"]