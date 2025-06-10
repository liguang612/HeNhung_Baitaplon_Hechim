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

- STM32F429-DISC1.
- Module: USSB2Serial.

## SO ĐỒ SCHEMATIC

_Cho biết cách nối dây, kết nối giữa các linh kiện_ 
Ví dụ có thể liệt kê dạng bảng
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

- Mô tả các thành phần phần cứng và vai trò của chúng: máy chủ, máy trạm, thiết bị IoT, MQTT Server, module cảm biến IoT...
- Mô tả các thành phần phần mềm và vai trò của chúng, vị trí nằm trên phần cứng nào: Front-end, Back-end, Worker, Middleware...

### ĐẶC TẢ HÀM

- Giải thích một số hàm quan trọng: ý nghĩa của hàm, tham số vào, ra

  ```C
     /**
      *  Hàm tính ...
      *  @param  x  Tham số
      *  @param  y  Tham số
      */
     void abc(int x, int y = 2);
  ```
  
### KẾT QUẢ

- Video minh họa:
![Video minh họa](https://github.com/Smo1003/Draft/releases/download/v1.0.0/Hechim.mp4)

