# EtherPlayer rev 2
Internet radio based on STM32F107

### Tasks
- [ ] Use FreeRTOS and socket lwIP API
  - [x] Add FreeRTOS to project
  - [x] Test existing libraries
  - [x] Test USB MSD Host and Ethernet
  - [x] Implement lwIP socket API
  - [x] Integrate memory friendly HTTP header parser with code
  - [ ] Move VS1003 feeding function to separate task
  - [x] Move buttons checking to separate task
- [ ] Create UI
  - [ ] Add display
  - [ ] Add rotary encoder
  - [ ] Write basic user interface
  
### Completed Column ✓
- [x] Move audio circular buffer back to SPI RAM
- [x] Add button library
- [x] Check SPI of VS1003
  - [x] Ensure that increasing speed after initial config is done right
  - [x] Check if it is possible to increase VS1003 SPI speed
- [x] Remove old circular buffer
- [x] Implement memory friendly HTTP header parser
