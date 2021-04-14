
BB-SHPRD-00A0.dtbo: BB-SHPRD-00A0.dtsp
	@echo ' '
	@echo '*******************  Compiling DT-File ************************'
	dtc -W no-unit_address_vs_reg -@ -I dts -O dtb -o $@ $<

BB-SHPRD-00A0.dtsp: BB-SHPRD-00A0.dts
	@echo ' '
	@echo '*******************  Precompiling DT-File *********************'
	cpp -nostdinc -I include -I arch  -undef -x assembler-with-cpp $< $@

.PHONY: all
all: BB-SHPRD-00A0.dtbo

.PHONY: clean
clean:
	@echo '*******************  Cleaning DT-Files ************************'
	rm -f BB-SHPRD-00A0.dtbo
	rm -f BB-SHPRD-00A0.dtsp
	@echo ' '

.PHONY: install
install: BB-SHPRD-00A0.dtbo
	@echo '*******************  Installing DT-Files **********************'
	install -D BB-SHPRD-00A0.dtbo \
		$(DESTDIR)/lib/firmware/BB-SHPRD-00A0.dtbo
