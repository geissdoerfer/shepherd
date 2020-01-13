TARGET = BB-GPS-00A0

$(TARGET).dtbo: $(TARGET).dts
	dtc -W no-unit_address_vs_reg -@ -I dts -O dtb -o $@ $<

install: $(TARGET).dtbo
	install -D $(TARGET).dtbo \
		$(DESTDIR)/lib/firmware/$(TARGET).dtbo

.PHONY: clean

clean:
	rm -f $(TARGET).dtbo
