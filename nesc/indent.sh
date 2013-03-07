#!/bin/bash
# vim: ts=4:et:sw=0:sts=-1

args="--indent-level4 \
      --no-space-after-function-call-names \
      --case-indentation4 \
      --braces-after-if-line \
      --no-tabs \
      --dont-break-procedure-type \
      \
      -T uses \
      -T provides \
      -T nx_struct \
      \
      -T nx_int8_t \
      -T nx_uint8_t \
      \
      -T nx_int16_t \
      -T nx_uint16_t \
      \
      -T nx_int32_t \
      -T nx_uint32_t \
      \
      -T nx_int64_t \
      -T nx_uint64_t \
      \
      -T message_t \
      -T node_msg_t \
      "

# we don't want the backup file
suffix=".ermahgerd_erndernt"
SIMPLE_BACKUP_SUFFIX=$suffix indent $args $@
find -name "*${suffix}" -delete

for f in $@; do
    # GNU indent turns "A -> B" to "A->B". Undo that for files containing configuration
    if grep -q '^configuration' $f; then
        sed -i -e 's/->/ -> /g' $f;
    fi
    # change "Interface < type >" back to "Interface<type>"
    sed -i -e 's/ < \(.*\) >/<\1>/' $f;
done
