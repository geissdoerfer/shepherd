.PHONY: all
all: BB-SHPRD-00A0.dtbo BB-SHPRDVCAP-00A0.dtbo

BB-SHPRD-00A0.dtbo: BB-SHPRD-00A0.dts
	dtc -W no-unit_address_vs_reg -@ -I dts -O dtb -o $@ $<

BB-SHPRDVCAP-00A0.dtbo: BB-SHPRDVCAP-00A0.dts
	dtc -W no-unit_address_vs_reg -@ -I dts -O dtb -o $@ $<

.PHONY: clean
clean:
	rm -f BB-SHPRD-00A0.dtbo
	rm -f BB-SHPRDVCAP-00A0.dtbo

.PHONY: install
install: BB-SHPRD-00A0.dtbo BB-SHPRDVCAP-00A0.dtbo
	install -D BB-SHPRD-00A0.dtbo \
		$(DESTDIR)/lib/firmware/BB-SHPRD-00A0.dtbo
	install -D BB-SHPRDVCAP-00A0.dtbo \
		$(DESTDIR)/lib/firmware/BB-SHPRDVCAP-00A0.dtbo
