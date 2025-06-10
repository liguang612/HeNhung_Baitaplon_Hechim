# Thiết bị giám sát và can thiệp đường truyển UART
  Nội dung bài tập lớn: Yêu cầu xây dựng một hệ thống truyền thông nối tiếp sử dụng vi điều khiển STM32F429 hoặc ESP32 kèm màn hình OLED, với ba kết nối UART: UART_A và UART_B dùng để truyền thông hai chiều giữa hai thiết bị, còn UART_H đóng vai trò giám sát toàn bộ dữ liệu truyền qua lại và nhận lệnh cấu hình từ thiết bị điều khiển. Hệ thống cho phép thay đổi baudrate của UART_A và UART_B thông qua UART_H, luôn cố định ở 115200, đồng thời hiển thị dữ liệu giám sát lên màn hình và lưu cấu hình baudrate vào bộ nhớ flash để tự động khôi phục sau mỗi lần khởi động.

## GIỚI THIỆU
  - Sử dụng STM32F429 hoặc ESP32 với màn hình OLED.
  - Khởi tạo đồng thời 3 kết nối UART, gọi là UART_A,UART_B, UART_H
  - Tất cả thông tin truyền nhận từ UART_A sẽ được nhận truyền sang UART_B và ngược lại. Như vậy 2 thiết bị liên lạc, là nạn nhân, ở 2 đầu A B vẫn liên lạc bình thường.
  - UART_H đóng vai trò giám sát. Mọi thông tin truyền đi ở UART_A và UART_B sẽ được đưa lên UART_H để truyền về thiết bị giám sát.
  - UART_H sẽ nhận lệnh điều khiển từ thiết bị giám sát để: thiết lập tốc độ baudrate cho UART_A và UART_B. Còn bản thân UART_H luôn được thiết lập ở tốc độ 115200.
  - Hiển thị thông tin giám sát lên màn hình
  - Lưu trữ cấu hình tốc độ của UART_A,B vào bộ nhớ flash vĩnh viễn để có thể tự động đọc lại mỗi khi chạy thiết bị
  - Ảnh chụp minh họa:\
    ![Ảnh minh họa](https://github.com/user-attachments/assets/50d8105f-984c-429c-a4ef-db2979660956)

## TÁC GIẢ

- Tên nhóm: Hệ chìm
- Thành viên trong nhóm

  |STT|Họ tên|MSSV|Công việc|
  |--:|--|--|--|
  |1|Bùi Anh Quốc |20215634|Khởi tạo kết nối, truyền thông tin giữa các UART, thiết lập baudrate|
  |2|Nguyễn Minh Sơn |20215637|Thiết kế mạch, lắp mạch, lưu trữ dữ liệu ra flash và load dữ liệu từ flash|
  |3|Bùi Quang Dũng |20215555|Hiển thị thông tin giám sát lên màn hình, làm báo cáo|

## MÔI TRƯỜNG HOẠT ĐỘNG

- STM32F429I-DISC1.
- Module: USSB2Serial.

## SƠ ĐỒ SCHEMATIC

_Cho biết cách nối dây, kết nối giữa các linh kiện_

|STM32F429|Module ngoại vi|Chân ngoại vi|
|--|--|--|
|Cổng debug|UART_H| |
|PB10|UART_A|RX|
|PB11|UART_A|TX|
|GND|UART_A|GND|
|PC7|UART_B|TX|
|PC6|UART_B|RX|
|GND|UART_B|GND|

### TÍCH HỢP HỆ THỐNG

#### Phần cứng

- Laptop/PC: Môi trường chạy phần mềm nạp code và hiển thị thông tin. Thiết bị giám sát, giao tiếp với UART_H
- STM32F429I-DISC1: UART_H, là trung gian truyền nhận thông tin giữa các UART, nhận lệnh từ thiết bị giám sát để thiết lập baud rate cho UART_A và UART_B. Cung cấp chân ngoại vi hỗ trợ kết nối UART
- Module USB2Serial: Đóng vai trò như UART_A và UART_B, sử dụng các chân ngoại vi của bộ kit STM32

#### Phần mềm

- STM32CubeIDE: Môi trường lập trình bộ kit STM32
- Hercules: Phần mềm hiển thị thông tin truyền nhận từ các UART

### ĐẶC TẢ HÀM

- Giải thích một số hàm quan trọng: ý nghĩa của hàm, tham số vào, ra

  ```C
     /**
      * Chuyển xâu về số. Hàm này chủ yếu dùng để lấy baudrate trong lệnh người dùng.
      *   @param str : xâu chứa số  
      *   Xâu chứa số hợp lệ: trả ra sô
      * 	Xâu không hợp lệ: return -1
      **/
      int extractNumber(char* str)
  ```
  ```C
     /**
      * Kiểm tra xem phần tử có nằm trong mảng không?  Hàm này chủ yếu dùng để kiểm tra số baudrate người dùng nhập có nằm trong danh sách hợp lệ không.
      *   @param mem : số cần tìm 
      *   @param arr : con trỏ mảng
      *   @param len : độ dài mảng
      **/
      bool includes(int* arr, int mem, int len)
  ```
  ```C
     /**
      * Chuyển xâu người dùng nhập về dạng lệnh và baudrate.
      *   @param cmd : Xâu chứa lệnh người dùng nhập vào
      *   @struct command : chứa loại lệnh và thông tin đi kèm (số baudrate)
      **/
      command extractCommand(char* cmd)
  ```
  ```C
     /**
      * Tính toán checksum dựa trên config
      *   @param config : Chứa config baudrate
      *   @struct UART_BaudRates_Config : chứa thông tin config (baudrate của UART_A, UART_B, số kiểm tra, checksum)
      **/
      uint32_t Calculate_Checksum(UART_BaudRates_Config* config)
  ```
  ```C
     /**
      * Lưu config vào bộ nhớ flash.
      *   @param config_to_save : Chứa config baudrate cần lưu
      **/
      void Save_Config_To_Flash(UART_BaudRates_Config* config_to_save)
  ```
  ```C
     /**
      * Lấy dữ liệu từ bộ nhớ flash.
      **/
      void Load_Config_From_Flash(void)
  ```
  ```C
     /**
      * Override việc xử lý ngắt được gây ra bởi DMA liên quan đến việc nhận dữ liệu từ các cổng UART (gửi bằng Hercules trên PC).
      **/
      void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size)
  ```

### KẾT QUẢ

- Video minh họa:
[![thumbnail](https://github.com/user-attachments/assets/0ac268a0-327d-4bbe-a3e8-fe414d70c5c6)](https://youtu.be/2pdPq1v_i2c)
