# EtherPlayer rev 2
Internet radio based on STM32F107

### Tasks
- [ ] Check SPI of VS1003
  - [ ] Ensure that increasing speed after initial config is done right
  - [x] Check if it is possible to increase VS1003 SPI speed
- [ ] Remove old circular buffer
- [ ] Use heap for header buffer
- [ ] Implement memory friendly HTTP header parser
- [ ] Feed VS1003 while saving received data (crucial for streaming at higher bitrates)
- [ ] Use FreeRTOS and socket lwIP API
  
### Completed Column ✓
- [x] Move audio circular buffer back to SPI RAM
- [x] Add button library
