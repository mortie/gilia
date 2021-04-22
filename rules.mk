objify = $(patsubst %,$(OUT)/%.o,$(1))
depify = $(patsubst %,$(OUT)/%.d,$(1))

$(OUT)/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ -c $<

$(OUT)/%.s.o: %.s
	@mkdir -p $(@D)
	$(AS) -o $@ -c $<

$(OUT)/%.c.d: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MT $(call objify,$<) -o $@ -MM $<

$(OUT)/%.s.d: %.s
	@mkdir -p $(@D)
	touch $@
