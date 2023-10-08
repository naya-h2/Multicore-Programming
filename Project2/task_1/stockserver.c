/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

void echo(int connfd);

typedef struct {
	int maxfd; //readset에서 fd 번호 제일 큰 거
	fd_set read_set; //active
	fd_set ready_set; //event
	int nready; //pending input이 발생한 fd의 개수, select()의 return값
	int maxi;
	int clientfd[FD_SETSIZE];
	rio_t clientrio[FD_SETSIZE];
}pool;

int client_num = 0; //server에 연결된 client 수
int byte_cnt = 0; //server로 받은 byte 수
char output[MAXLINE]; //client server에 print할 내용

typedef struct item{
	int ID;
	int left_stock; //left stock의 수
	int price;
	int readcnt;
	sem_t mutex;
}item;

typedef struct node{
	item data;
	struct node* left; //left child(small)
	struct node* right; //right child(big)
}node;

node* root = NULL; //첫 번째 node
node* cur_node;

node* insert(item new, node* cur_node){ //tree에 stock 정보 삽입
	//new를 cur_node에 붙이기
	if(cur_node == NULL){ //연결된 data 없으면
		cur_node = (node*)malloc(sizeof(node));
		cur_node->data = new;
		cur_node->left = NULL;
		cur_node->right = NULL;
		return cur_node;
	}
	else{
		if(new.ID < (cur_node->data).ID){ //작은 애는 left로
			cur_node->left = insert(new, cur_node->left);
		}
		else if(new.ID > (cur_node->data).ID){ //큰 애는 right로
			cur_node->right = insert(new, cur_node->right);
		}
	}
	//printf("cur_node : %d %d %d\n", (cur_node->data).ID, (cur_node->data).left_stock, (cur_node->data).price);
	
	return cur_node;
}

void show(node* cur){
	if(cur == NULL) return;

	item cur_item = cur->data;
	//결과 문자열 1개로 합치기
	char str[MAXLINE];

	sprintf(str, "%d %d %d\n", cur_item.ID, cur_item.left_stock, cur_item.price);
	strcat(output, str);

	//fprintf(stdout, "%s", str);
	//아니 왜 한줄밖에 안나옴?
	//Rio_writen(connfd, str, strlen(str));

	show(cur->left);
	show(cur->right);
}

node* search(node* cur_node, int id){
	if(cur_node == NULL) return NULL;
	while(cur_node){
		if(id == (cur_node->data).ID){
			return cur_node;
		}
		else{
			if(id < (cur_node->data).ID){
				cur_node = cur_node->left;
			}
			else cur_node = cur_node->right;
		}
	}
	return NULL; //없는 경우
}

void save(node* cur_node, FILE* fp){
	if(cur_node == NULL) return;
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

void init_pool(int listenfd, pool *p){
	/*Initially, there are no connected descriptors */
	p->maxi = -1;
	for(int i = 0; i < FD_SETSIZE; i++){
		p->clientfd[i] = -1; //초기엔 connfd 없으니까 -1로 초기화
	}
	
	/* Initially, listenfd is only member of select read set */
	p->maxfd = listenfd;
	FD_ZERO(&p->read_set); //read_set 0으로 초기화
	FD_SET(listenfd, &p->read_set); //listenfd를 active로 설정
}

void add_client(int connfd, pool *p){ //add client to the pool
	int i;
	p->nready--; //event 하나 처리했으니까 --
	client_num++;
	for(i = 0; i < FD_SETSIZE; i++){
		if(p->clientfd[i] < 0){ //empty slot일 때
			/* Add connected fd to the pool */
			p->clientfd[i] = connfd;
			Rio_readinitb(&p->clientrio[i], connfd);

			/* Add the fd to fd set */
			FD_SET(connfd, &p->read_set); //connfd를 active 상태로

			/* Update maxfd & maxi */
			if(connfd > p->maxfd)
				p->maxfd = connfd;
			if(i > p->maxi)
				p->maxi = i;
			break;
		}
	}
	if(i == FD_SETSIZE) //empty slot 못 찾은 경우
		app_error("add_client error: Too many clients");
}

void check_clients(pool *p){
	int i, connfd, n;
	char buf[MAXLINE];
	rio_t rio;

	for(i = 0; (i <= p->maxi) && (p->nready > 0); i++){ //nready가 남아있는 경우
		connfd = p->clientfd[i];
		rio = p->clientrio[i];

		/* If the fd is ready, echo a text line from it */
		if((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))){ //connfd에 event 발생한 경우
			p->nready--;

			if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
				byte_cnt += n;
				printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
				//Rio_writen(connfd, buf, n);
				strcat(output, buf); //echo

				char cmd[10]; int id, num;
				sscanf(buf, "%s %d %d", cmd, &id, &num);
				//printf("%s %d %d\n", cmd, id, num);
				
				//1. show
				if(!strcmp(cmd, "show")){
					show(root);
					//printf("%s", output);;
					//printf("%ld", length);
					Rio_writen(connfd, output, sizeof(output));
				}
				
				//2. buy [id] [num]
				else if(!strcmp(cmd, "buy")){
					node* nd = search(root, id); //buy할 stock node 찾기
					if(nd == NULL){
						strcat(output, "Stock does not exist\n");
					}
					else if((nd->data).left_stock < num){
						strcat(output, "Not enough left stock\n");
					}
					else{
						(nd->data).left_stock -= num;
						strcat(output, "[buy] success\n");
					}
					//printf("%s\n", buf);
					Rio_writen(connfd, output, sizeof(output));
				}

				//3. sell [id] [num]
				else if(!strcmp(cmd, "sell")){
					node* nd = search(root, id);
					if(nd == NULL){
						strcat(output, "Stock does not exist\n");
					}
					else{
						(nd->data).left_stock += num;
						strcat(output, "[sell] success\n");
					}
					//printf("%s\n", output);
					Rio_writen(connfd, output, sizeof(output));
				}
				/*else if(!strcmp(cmd, "exit")){
					Close(connfd);
					FD_CLR(connfd, &p->read_set);
					p->clientfd[i] = -1;
					sigint_handler(SIGINT);
				}*/
				memset(output, 0, sizeof(output));
			}
			else{
				Close(connfd);
				FD_CLR(connfd, &p->read_set); //connfd를 inactive 상태로
				p->clientfd[i] = -1; //clientfd(connfd array)에서 삭제
				client_num--;
			}
		}
	}
}

void sigint_handler(int signo){
	FILE* fp = fopen("stock.txt", "w");
	if(fp == NULL){
		printf("file open error!\n");
		exit(1);
	}
	save(root, fp);
	printf("save\n");
	fclose(fp);
	freenode(root);
	printf("free\n");
	exit(1);
}

int main(int argc, char **argv) 
{
	Signal(SIGINT, sigint_handler); //ctrl+c
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
	static pool pool;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
	
	//FILE OPEN
	FILE* fp = fopen("stock.txt", "r");
	if(fp == NULL){ //error 처리
		printf("File open error!\n");
		return 0;
	}
	

	int s_id, s_left, s_price;
	while(fscanf(fp, "%d %d %d\n", &s_id, &s_left, &s_price) != EOF){
		//file에서 읽어온 stock 정보 item으로 저장
		item new;
		
		new.ID = s_id;
		new.left_stock = s_left;
		new.price = s_price;
		new.readcnt = 0;

		if(root == NULL){
			root = (node*)malloc(sizeof(node));
			root->data = new;
			root->left = NULL;
			root->right = NULL;
		}
		root = insert(new, root);
	}
	fclose(fp); //file 다 저장했으니까 close

	//print_tree(0, root); //print tree

    listenfd = Open_listenfd(argv[1]); //client 요청 기다리는 상태
	init_pool(listenfd, &pool); //pool 초기화

    while (1) {
		/* wait for listening/connected fd */
		pool.ready_set = pool.read_set; //read_set copy
		pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL); //pending input 여부 확인
		
		/* If listening fd ready, add new client to pool */
		if(FD_ISSET(listenfd, &pool.ready_set)){ //listenfd에 event 발생
			clientlen = sizeof(struct sockaddr_storage);
			connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //connect 맺어짐
			add_client(connfd, &pool); //pool에 생성된 connfd 넣기
			Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
			printf("Connected to (%s, %s)\n", client_hostname, client_port);
		}
		
		/* Echo a text line from each ready connected fd */
		check_clients(&pool); //server 기능 구현
		if(!client_num) sigint_handler(SIGINT);
		
 		/*
		echo(connfd);
		Close(connfd);*/
    }
    exit(0);
}
/* $end echoserverimain */
