ARG IMAGE
FROM $IMAGE

COPY --chown=999:999 ./configs /configs
# USER root

# RUN mkdir /config
# RUN chown -R 999 /config

# USER 999:999