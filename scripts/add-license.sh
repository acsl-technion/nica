#!/bin/bash

docker run --rm --volume `pwd`:/usr/src/ osterman/copyright-header:latest \
    --license BSD-2-CLAUSE \
    --add-path . \
    --guess-extension \
    --copyright-holder 'Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork' \
    --copyright-software 'NICA' \
    --copyright-software-description 'Framework for NIC application acceleration on the FPGA' \
    --copyright-year 2016-2017 \
    --word-wrap 80 \
    --output-dir /usr/src/
