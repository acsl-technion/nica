all: vivado

TB_FILES=passthrough.pcap passthrough-padded.pcap input.pcap input-padded.pcap

export: $(TB_FILES)
	vivado_hls -f nica.tcl

simulation: $(TB_FILES)
	env SIMULATION_BUILD=1 vivado_hls -f nica.tcl

%.pcap: tests/%.pcap
	cp $< $@

%-padded.pcap: %.pcap tests/pad_small_packets.py
	python tests/pad_small_packets.py $< $@

vivado: simulation export

.PHONY: subdirs export simulation vivado
