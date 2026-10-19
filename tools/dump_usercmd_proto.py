import explore_client as ec

off = ec.va_to_offset(0x181BCC906)
print("CSGOUserCmdPB protobuf descriptor:")
print(repr(ec.buf[off:off+600]))
