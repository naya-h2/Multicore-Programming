/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define NTHREADS 100
#define SBUFSIZE 100

static sem_t mutex; //byte_cnt, client_num

typedef struct{
	int *buf;
	int n; //buf에 넣을 수 있는 item 개수
	int front;
	int rear;
	sem_t mutex;
	sem_t slots; //buf의 비어있는 slot 개수
	sem_t items; //buf에 있는 item 개수
}sbuf_t;

sbuf_t sbuf;
int client_num = 0; //server에 연결된 client 수
static int byte_cnt = 0; //server로 받은 byte 수
char output[MAXLINE]; //client server에 print할 내용

typedef struct item {
	int ID;
	int left_stock; //left stock의 수
	int price;
	int readcnt; //reader 수
	sem_t mutex; //readcnt
	sem_t cs; //left_stock
}item;

typedef struct node {
	item data;
	struct node* left; //left child(small)
	struct node* right; //right child(big)
}node;

node* root = NULL; //첫 번째 node

void sbuf_init(sbuf_t *sp, int n){
	sp->buf = Calloc(n, sizeof(int)); // n = buf 수
	sp->n = n;
	sp->front = sp->rear = 0; //empty buffer
	Sem_init(&sp->mutex, 0, 1);
	Sem_init(&sp->slots, 0, n); //n개의 empty slot
	Sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(sbuf_t *sp){
	Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item){
	P(&sp->slots);
	P(&sp->mutex); //lock
	sp->buf[(++sp->rear)%(sp->n)] = item; //critical section
	V(&sp->mutex); //unlock
	V(&sp->items);
}

int sbuf_remove(sbuf_t *sp){
	int item;
	P(&sp->items);
	P(&sp->mutex); //lock
	item = sp->buf[(++sp->front)%(sp->n)];
	V(&sp->mutex); //unlock
	V(&sp->slots);
	return item;
}

node* insert(item new, node* cur_node) { //tree에 stock 정보 삽입
	//new를 cur_node에 붙이기
	if (cur_node == NULL) { //연결된 data 없으면
		cur_node = (node*)malloc(sizeof(node));
		Sem_init(&(new.mutex), 0, 1);
		Sem_init(&(new.cs), 0, 1);
		cur_node->data = new;
		cur_node->left = NULL;
		cur_node->right = NULL;
		return cur_node;
	}
	else {
		if (new.ID < (cur_node->data).ID) { //작은 애는 left로
			cur_node->left = insert(new, cur_node->left);
		}
		else if (new.ID > (cur_node->data).ID) { //큰 애는 right로
			cur_node->right = insert(new, cur_node->right);
		}
	}
	//printf("cur_node : %d %d %d\n", (cur_node->data).ID, (cur_node->data).left_stock, (cur_node->data).price);
	return cur_node;
}

void show(node* cur) {
	if (cur == NULL) return;

	item cur_item = cur->data;
	//결과 문자열 1개로 합치기
	char str[MAXLINE];

	sprintf(str, "%d %d %d\n", cur_item.ID, cur_item.left_stock, cur_item.price);
	strcat(output, str);

	//printf("show() == %s", output);

	show(cur->left);
	show(cur->right);
}

node* search(node* cur_node, int id) {
	if (cur_node == NULL) return NULL;
	while (cur_node) {
		if (id == (cur_node->data).ID) {
			return cur_node;
		}
		else {
			if (id < (cur_node->data).ID) {
				cur_node = cur_node->left;
			}
			else cur_node = cur_node->right;
		}
	}
	return NULL; //없는 경우
}

void save(node* cur_node, FILE* fp) {
	if (cur_node == NULL) return;
	fprintf(fp, "%d %d %d\n", (cur_node->data).ID, (cur_node->data).left_stock, (cur_node->data).price);
	save(cur_node->left, fp);
	save(cur_node->right, fp);
	return;
}

void freenode(node* cur_node){
	if(cur_node == NULL) return;
	freenode(cur_node->left);
	freenode(cur_node->right);
	free(cur_node);
	return;
}
static void init_stock(void)
{
	Sem_init(&mutex, 0, 1);
	byte_cnt = 0;
}
void func(int connfd)
{
	int n;
	char buf[MAXLINE];
	rio_t rio;
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	Pthread_once(&once, init_stock);

	Rio_readinitb(&rio, connfd);
	while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
		byte_cnt += n;
		printf("thread %d received %d (%d total) bytes on fd %d\n",
			(int)pthread_self(), n, byte_cnt, connfd);
	
		//Rio_writen(connfd, buf, n);
		strcat(output, buf); //echo 결과

		char cmd[10]; int id, num;
		sscanf(buf, "%s %d %d", cmd, &id, &num);
		//printf("%s %d %d\n", cmd, id, num);

		//1. show
		if (!strcmp(cmd, "show")) {
			P(&mutex);
			show(root);
			//printf("%s", output);
			//size_t length = strlen(output);
			//printf("%ld", length);
			Rio_writen(connfd, output, sizeof(output));
			V(&mutex);
		}

		//2. buy [id] [num]
		else if (!strcmp(cmd, "buy")) {
			P(&mutex);
			node* nd = search(root, id); //buy할 stock node 찾기
			if (nd == NULL) {
				strcat(output, "Stock does not exist\n");
			}
			else if ((nd->data).left_stock < num) {
				//printf("buy mutex lock\n");
				strcat(output, "Not enough left stock\n");
			}
			else {
				(nd->data).left_stock -= num;
				strcat(output, "[buy] success\n");
			}
			//printf("%s\n", buf);
			Rio_writen(connfd, output, sizeof(output));
			V(&mutex);
			//free(nd);
		}

		//3. sell [id] [num]
		else if (!strcmp(cmd, "sell")) {
			P(&mutex);
			node* nd = search(root, id);
			if (nd == NULL) {
				strcat(output, "Stock does not exist\n");
			}
			else {
				(nd->data).left_stock += num;
				strcat(output, "[sell] success\n");
			}
			//printf("%s\n", output);
			Rio_writen(connfd, output, sizeof(output));
			V(&mutex);
		}
		memset(output, 0, sizeof(output));
	}
}

void sigint_handler(int signo) {
	FILE* fp = fopen("stock.txt", "w");
	if (fp == NULL) {
		printf("file open error!\n");
		exit(1);
	}
	init_stock();
	//printf("끝낼 때 %d\n", mutex);
	P(&mutex);
	save(root, fp);
	printf("save\n");
	fclose(fp);
	freenode(root);
	printf("free\n");
	V(&mutex);
	exit(1);
}
void* thread(void* vargp) {
	Pthread_detach(pthread_self());
	while (1) {
		int connfd = sbuf_remove(&sbuf); /* Remove connfd from buf */
		init_stock();	
		//printf("thread %d\n", connfd);
		P(&mutex);
		client_num++;
		V(&mutex);
		//printf("client_num %d\n", client_num);

		func(connfd); /* Service client */
		Close(connfd);
		P(&mutex);
		client_num--;
		if(!client_num){
			sigint_handler(SIGINT); //정보 저장
		}
		V(&mutex);
	}
}

int main(int argc, char **argv) 
{
	Signal(SIGINT, sigint_handler);
	int i, listenfd, connfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	pthread_t tid;
	char client_hostname[MAXLINE], client_port[MAXLINE];

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	//FILE OPEN
	FILE* fp = fopen("stock.txt", "r");
	if (fp == NULL) { //error 처리
		printf("File open error!\n");
		return 0;
	}

	//stock 정보 tree로 저장
	int s_id, s_left, s_price;
	while (fscanf(fp, "%d %d %d\n", &s_id, &s_left, &s_price) != EOF) {
		//file에서 읽어온 stock 정보 item으로 저장
		item new;

		new.ID = s_id;
		new.left_stock = s_left;
		new.price = s_price;
		new.readcnt = 0;
		if (root == NULL) {
			root = (node*)malloc(sizeof(node));
			root->data = new;
			root->left = NULL;
			root->right = NULL;
		}
		root = insert(new, root);
	}
	fclose(fp); //file 다 저장했으니까 close

	listenfd = Open_listenfd(argv[1]);
	sbuf_init(&sbuf, SBUFSIZE);
	
	for (i = 0; i < NTHREADS; i++) /* Create a pool of worker threads */
		Pthread_create(&tid, NULL, thread, NULL); //미리 thread 만들기
	while (1) { //connect 기다림
		clientlen = sizeof(struct sockaddr_storage);
		connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
		Getnameinfo((SA*)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
		printf("Connected to (%s, %s)\n", client_hostname, client_port);
		sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
	}

    exit(0);
}
/* $end echoserverimain */
