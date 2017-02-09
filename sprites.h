const static byte spv_1[] PROGMEM = {0x00, 0x10, 0x12, 0x02};
const static byte spv_2[] PROGMEM = {0x21, 0x31, 0x32, 0x22};
const static byte spv_3[] PROGMEM = {0x02, 0x12, 0x22, 0x32, 0x62, 0x63, 0x03};

const static sprite_polygon sp_1[] PROGMEM = {
  {4, 0x0b, spv_1},
  {4, 0x0b, spv_2},
  {7, 0x6a, spv_3}
};
const static sprite sprites[] PROGMEM = {
  {3, sp_1}
};
