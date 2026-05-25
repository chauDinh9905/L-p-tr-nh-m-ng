#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

// Cấu trúc dữ liệu để truyền thông tin phòng chat (cặp đôi) vào luồng
typedef struct {
    int client1;
    int client2;
} ChatPair;

void *chat_bridge_thread(void *);

// Biến toàn cục quản lý hàng đợi đơn giản (chỉ cần 1 vị trí chờ)
int waiting_client = -1; 
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // Mutex để bảo vệ biến dùng chung

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }
    
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8888); // Chạy thử ở port 8888
    
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }
    
    if (listen(listener, 10)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }
    
    printf("Pairing Chat Server is listening on port 8888...\n");
    
    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;
        
        printf("Client %d connected. Checking queue...\n", client);
        
        pthread_mutex_lock(&lock);
        if (waiting_client == -1) {
            // Yêu cầu 1a: Chưa có ai chờ -> Đưa client này vào hàng đợi
            waiting_client = client;
            char msg[] = "Welcome! Waiting for a partner to join...\n";
            send(client, msg, strlen(msg), 0);
            pthread_mutex_unlock(&lock);
        } else {
            // Yêu cầu 1b: Đã có 1 người chờ -> Đủ 2 người, tiến hành ghép cặp!
            int c1 = waiting_client;
            int c2 = client;
            waiting_client = -1; // Reset hàng đợi về trống
            pthread_mutex_unlock(&lock);
            
            char match_msg[] = "Partner found! Chat connected.\n";
            send(c1, match_msg, strlen(match_msg), 0);
            send(c2, match_msg, strlen(match_msg), 0);
            
            printf("Paired Client %d and Client %d successfully.\n", c1, c2);
            
            // Cấp phát động cấu trúc cặp đôi để truyền vào luồng độc lập
            ChatPair *pair1 = malloc(sizeof(ChatPair));
            pair1->client1 = c1; // Người gửi
            pair1->client2 = c2; // Người nhận
            
            ChatPair *pair2 = malloc(sizeof(ChatPair));
            pair2->client1 = c2; // Người gửi
            pair2->client2 = c1; // Người nhận
            
            // Tạo 2 luồng song song để chuyển tiếp tin nhắn qua lại (Full-duplex)
            pthread_t t1, t2;
            pthread_create(&t1, NULL, chat_bridge_thread, pair1);
            pthread_create(&t2, NULL, chat_bridge_thread, pair2);
            
            // Detach để hệ thống tự giải phóng tài nguyên luồng khi hoàn thành
            pthread_detach(t1);
            pthread_detach(t2);
        }
    }
    
    close(listener);
    return 0;
}

// Luồng nhận tin nhắn từ client1 và chuyển tiếp thẳng sang client2
void *chat_bridge_thread(void *params) {
    ChatPair *pair = (ChatPair *)params;
    int sender = pair->client1;
    int receiver = pair->client2;
    free(pair); // Giải phóng bộ nhớ của cấu trúc tạm
    
    char buf[2048];
    
    while (1) {
        int len = recv(sender, buf, sizeof(buf), 0);
        
        // Yêu cầu 3: Nếu 1 trong 2 ngắt kết nối (len <= 0)
        if (len <= 0) {
            printf("Client %d disconnected. Closing room.\n", sender);
            
            // Gửi thông báo cho client còn lại biết và đóng luôn kết nối của họ
            char bye_msg[] = "Your partner disconnected. Chat ended.\n";
            send(receiver, bye_msg, strlen(bye_msg), 0);
            
            // Đóng socket của cả hai người
            close(sender);
            close(receiver);
            break; 
        }
        
        // Yêu cầu 2: Chuyển tiếp tin nhắn từ client này sang client kia
        send(receiver, buf, len, 0);
    }
    
    return NULL;
}