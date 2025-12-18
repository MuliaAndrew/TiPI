# must be built from deploy dir
ARG IMAGE
FROM $IMAGE

COPY --chown=999:999 ./configs /configs