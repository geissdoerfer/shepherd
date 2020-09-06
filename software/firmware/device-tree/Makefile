.PHONY: all
all: BB-SHPRD-00A0.dtbo BB-SHPRDVCAP-00A0.dtbo

BB-SHPRD-00A0.dtbo: BB-SHPRD-00A0.dtsp
	dtc -W no-unit_address_vs_reg -@ -I dts -O dtb -o $@ $<

BB-SHPRDVCAP-00A0.dtbo: BB-SHPRDVCAP-00A0.dtsp
	dtc -W no-unit_address_vs_reg -@ -I dts -O dtb -o $@ $<

BB-SHPRD-00A0.dtsp: BB-SHPRD-00A0.dts
	cpp -nostdinc -I include -I arch  -undef -x assembler-with-cpp $< $@

BB-SHPRDVCAP-00A0.dtsp: BB-SHPRDVCAP-00A0.dts
	cpp -nostdinc -I include -I arch  -undef -x assembler-with-cpp $< $@

.PHONY: clean
clean:
	rm -f BB-SHPRD-00A0.dtbo
	rm -f BB-SHPRDVCAP-00A0.dtbo
	rm -f BB-SHPRD-00A0.dtsp
	rm -f BB-SHPRDVCAP-00A0.dtsp

.PHONY: install
install: BB-SHPRD-00A0.dtbo BB-SHPRDVCAP-00A0.dtbo
	install -D BB-SHPRD-00A0.dtbo \
		$(DESTDIR)/lib/firmware/BB-SHPRD-00A0.dtbo
	install -D BB-SHPRDVCAP-00A0.dtbo \
		$(DESTDIR)/lib/firmware/BB-SHPRDVCAP-00A0.dtbo
