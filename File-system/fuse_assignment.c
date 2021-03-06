

/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall rmfs.c `pkg-config fuse --cflags --libs` -o rmfs
*/

#define FUSE_USE_VERSION 26
#define FILENAME_SIZE 30
#define FULLPATHNAME 1000 

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include "fuse_common.h"

/*
Initially all the function were to make sure reusability is maintain, but method switching cost is verfied after
running postmark program. Hence lookup code is repeated in the all the function to make it faster.
*/


typedef enum {Nfile, Ndir} Ntype;

struct node_t {		
  Ntype type;
  char name[FILENAME_SIZE];		
  struct node_t *child;	
  struct node_t *next;
  char *data;
  int len;
};
typedef struct node_t *Node;

Node root,buffNode;
long long malloc_counter=0,malloc_limit=0;

int ckmalloc(unsigned l,Node *t)
{
  void *p;
  if(malloc_limit < (malloc_counter+l))
  {
    return ENOSPC;
  }  

  p = malloc(l);
  if ( p == NULL ) {
    perror("malloc");
    return errno;
  }
  memset(p,'\0',l);
  *t=p;
  malloc_counter+=l;

  return 0;
}

 int ckmalloc_w(unsigned l,char **t)
{
  void *p;

  if(malloc_limit < (malloc_counter+l))
  {
    return ENOSPC;
  }

  p = malloc(l);
  if ( p == NULL ) {
    perror("malloc");
    return errno;
  }
  memset(p,'\0',l);
  *t=p;
  malloc_counter+=l;
  return 0;
}

void freemalloc(Node n)
{
  long totalsize=0;
  if(n->data!=NULL)
  {
    totalsize+=n->len;
    free (n->data);
  }
  totalsize+=sizeof(*root);
  free(n);
  malloc_counter-=totalsize;
}


static int directory_lookup(const char *path,Node *t,int mode)
{
	char lpath[100];
	memset(lpath,'\0',100);
	memcpy(lpath,path,strlen(path));	

	char *token=strtok(lpath, "/");
	Node temp=root,prev=NULL;

    while( token != NULL) 
    {
      //printf("inside the loop\n");
      prev=temp;
      temp=temp->child;
     // printf( " <%s>\n", token );

      while(temp!=NULL) {
      	if (strcmp(token, temp->name) == 0) {
      		printf("found %s \n",temp->name );
      		break;
        }      
      	temp=temp->next; 
  	  }
      if(temp==NULL)
      {
      	if(mode==1) *t=prev;
      	return -1;
      }

      token = strtok(NULL, "/");
    }    

    *t=temp;
    return 0;
}

/*
static int delete_node(const char *path,Ntype t)
{
//	printf("inside the delete_node %s\n",path);
	char lpath[100];
	memset(lpath,'\0',100);
	memcpy(lpath,path,strlen(path));	

	char *token=strtok(lpath, "/");
	Node temp=root,prev=NULL,sib;

	while( token != NULL) 
    {
	  prev=temp;sib=NULL;
      temp=temp->child;

      while(temp!=NULL) {
      	if (strcmp(token, temp->name) == 0) {
      		printf("found %s \n",temp->name );
      		break;
        }
        sib=temp;      
      	temp=temp->next; 
  	  }
      if(temp==NULL)
      {
      	return -1;
      }
      token = strtok(NULL, "/");
    }

    if(temp->type==Ndir && temp->child != NULL)
    {
       return ENOTEMPTY;
    }

    if(sib==NULL)
    {
    	prev->child=temp->next;
    }
    else if(sib!=NULL)
    {
    	sib->next=temp->next;
    }


    freemalloc(temp);
    return 0;
}
*/

static int rmfs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
  Node temp=root;
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} 
  else {  
    char lpath[100];
    memset(lpath,'\0',100);
    memcpy(lpath,path,strlen(path));  

    char *token=strtok(lpath, "/");

    while( token != NULL) 
    {
      temp=temp->child;
      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
          //printf("found %s \n",temp->name );
          break;
        }      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        break;
      }

      token = strtok(NULL, "/");
    }    
    
    if(temp!=NULL)
    {
		  stbuf->st_mode = S_IFREG | 0777;
		  stbuf->st_size = 0;
		  if(temp->type==Ndir)
		  	stbuf->st_mode = S_IFDIR | 0777;
		  else if(temp->data !=NULL)
			 stbuf->st_size=strlen(temp->data);
		  stbuf->st_nlink = 1;
		}
	 else 
		  res = -ENOENT;
  }
  buffNode=temp;
  //printf("<<<<<<< %lld  , %lld>>>>>>>\n",malloc_counter,malloc_limit );
	return res;
}

static int rmfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;
  char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node dirNode=root;

  while( token != NULL) 
  {
    dirNode=dirNode->child;
    while(dirNode!=NULL) {
      if (strcmp(token, dirNode->name) == 0) {
          //printf("found %s \n",dirNode->name );
          break;
      }      
        dirNode=dirNode->next; 
    }
    if(dirNode==NULL)
    {
      break;
    }
    token = strtok(NULL, "/");
  }

  if(dirNode==NULL)
    return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

  dirNode=dirNode->child;
  while(dirNode!=NULL)
  {
    //printf("node dir %s\n",dirNode->name);
		filler(buf, dirNode->name, NULL, 0);
		dirNode=dirNode->next;
  }

	return 0;
}
	
static int rmfs_open(const char *path, struct fuse_file_info *fi)
{
	char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node dirNode=root;

  while( token != NULL) 
  {
    dirNode=dirNode->child;
    while(dirNode!=NULL) {
      if (strcmp(token, dirNode->name) == 0) {
          //printf("found %s \n",dirNode->name );
          break;
      }      
        dirNode=dirNode->next; 
    }
    if(dirNode==NULL)
    {
      break;
    }
    token = strtok(NULL, "/");
  }

  if(dirNode==NULL)
    return -ENOENT;

	return 0;
}

static int rmfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	 char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node dirNode=root;

  while( token != NULL) 
  {
    dirNode=dirNode->child;
    while(dirNode!=NULL) {
      if (strcmp(token, dirNode->name) == 0) {
          //printf("found %s \n",dirNode->name );
          break;
      }      
        dirNode=dirNode->next; 
    }
    if(dirNode==NULL)
    {
      break;
    }
    token = strtok(NULL, "/");
  }

  if(dirNode==NULL)
    return -ENOENT;

  if(dirNode->type==Ndir)
    	return -EISDIR;

  if(dirNode->data==NULL)
    	return 0;

	len = strlen(dirNode->data);

	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
	//	memcpy(buf, rmfs_str + offset, size);
		memcpy(buf, dirNode->data + offset, size);
	} else
		size = 0;

	//printf("in the read buf : <%s> and size <%d>\n", buf,size);
	return size;
}

static void getFileName(const char *path,char lastname[])
{
	char spiltstr[100];
	memset(spiltstr,'\0',100);
	memcpy(spiltstr,path,strlen(path));
	char *token=strtok(spiltstr, "/");
	char *lastslsh;
    while( token != NULL ) 
    {
    	lastslsh=token;
    	token = strtok(NULL, "/");
    }	
	strncpy(lastname,lastslsh,strlen(lastslsh));
    return;
}

/*
static void getLastChild(Node *t)
{
	Node child=*t;
	child=child->child;
	while(child->next!=NULL)
    {
    //	printf("inside the getlastchildloop %s\n",child->name);
		child=child->next;
    }
    *t=child;
}
*/

static int rmfs_mkdir(const char *path, mode_t mode)
{
	Node dirNode=NULL;
	Node newDirNode=NULL;
	char a[FILENAME_SIZE];
	memset(a,'\0',FILENAME_SIZE);
  char spiltstr[100];
  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  char *token=strtok(spiltstr, "/");
  char *lastslsh;
  while( token != NULL ) 
  {
    lastslsh=token;
    token = strtok(NULL, "/");
  } 
  strncpy(a,lastslsh,strlen(lastslsh));

  if(malloc_limit < (malloc_counter+sizeof(*root)))
  {
    return -ENOSPC;
  }

  newDirNode = malloc(sizeof(*root));
  if ( newDirNode == NULL ) {
    perror("malloc");
    return errno;
  }
  malloc_counter+=sizeof(*root);

  memset(newDirNode,'\0',sizeof(*root));

  newDirNode->type=Ndir;
  strncpy(newDirNode->name,a,sizeof(a));

  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  token=strtok(spiltstr, "/");

  Node temp=root,prev=NULL;

  while( token != NULL) 
  {
      //printf("inside the loop\n");
      prev=temp;
      temp=temp->child;
      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
          //printf("found %s \n",temp->name );
          break;
        }      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        break;
      }
      token = strtok(NULL, "/");
  }    

  dirNode=prev;

  if(prev->child !=NULL)
  {	
    prev=prev->child;
    while(prev->next!=NULL)
    {
      //  printf("inside the getlastchildloop %s\n",child->name);
      prev=prev->next;
    }
    dirNode=prev;
  	dirNode->next=newDirNode;
  	//printf("name is : %s\n",dirNode->next->name );
  }
  else
  {
    dirNode->child=newDirNode;
   // printf("name c is : %s\n",dirNode->child->name );
  }
	return 0;
}

static int rmfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	Node dirNode=NULL;
  Node newDirNode=NULL;
  char a[FILENAME_SIZE];
  memset(a,'\0',FILENAME_SIZE);
  char spiltstr[100];
  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  char *token=strtok(spiltstr, "/");
  char *lastslsh;
  while( token != NULL ) 
  {
    lastslsh=token;
    token = strtok(NULL, "/");
  } 
  strncpy(a,lastslsh,strlen(lastslsh));

  if(malloc_limit < (malloc_counter+sizeof(*root)))
  {
    return -ENOSPC;
  }

  newDirNode = malloc(sizeof(*root));
  if ( newDirNode == NULL ) {
    perror("malloc");
    return errno;
  }
  malloc_counter+=sizeof(*root);
  memset(newDirNode,'\0',sizeof(*root));

  newDirNode->type=Nfile;
  strncpy(newDirNode->name,a,sizeof(a));

  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  token=strtok(spiltstr, "/");

  Node temp=root,prev=NULL;

  while( token != NULL) 
  {
      //printf("inside the loop\n");
      prev=temp;
      temp=temp->child;
      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
          //printf("found %s \n",temp->name );
          break;
        }      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        break;
      }
      token = strtok(NULL, "/");
  }    

  dirNode=prev;

  if(prev->child !=NULL)
  { 
    prev=prev->child;
    while(prev->next!=NULL)
    {
      //  printf("inside the getlastchildloop %s\n",child->name);
      prev=prev->next;
    }
    dirNode=prev;
    dirNode->next=newDirNode;
    //printf("name is : %s\n",dirNode->next->name );
  }
  else
  {
    dirNode->child=newDirNode;
   // printf("name c is : %s\n",dirNode->child->name );
  }
  return 0;
}

static int rmfs_flush(const char *path, struct fuse_file_info *fi)
{ 	/*
	printf("printing all root childs\n");
	Node tcp=root;
	tcp=tcp->child;

	while(tcp !=NULL)
	{
		printf("node -> %s\n",tcp->name );
		tcp=tcp->next;
	}
	*/
	return 0;
}

static int rmfs_create(const char *path, mode_t t,struct fuse_file_info *fi)
{ 	
  Node dirNode=NULL;
  Node newDirNode=NULL;
  char a[FILENAME_SIZE];
  memset(a,'\0',FILENAME_SIZE);
  char spiltstr[100];
  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  char *token=strtok(spiltstr, "/");
  char *lastslsh;
  while( token != NULL ) 
  {
    lastslsh=token;
    token = strtok(NULL, "/");
  } 
  strncpy(a,lastslsh,strlen(lastslsh));

 if(malloc_limit < (malloc_counter+sizeof(*root)))
  {
    return -ENOSPC;
  }

  newDirNode = malloc(sizeof(*root));
  if ( newDirNode == NULL ) {
    perror("malloc");
    return errno;
  }
  malloc_counter+=sizeof(*root);
  memset(newDirNode,'\0',sizeof(*root));

  newDirNode->type=Nfile;
  strncpy(newDirNode->name,a,sizeof(a));

  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  token=strtok(spiltstr, "/");

  Node temp=root,prev=NULL;

  while( token != NULL) 
  {
      //printf("inside the loop\n");
      prev=temp;
      temp=temp->child;
      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
          //printf("found %s \n",temp->name );
          break;
        }      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        break;
      }
      token = strtok(NULL, "/");
  }    

  dirNode=prev;

  if(prev->child !=NULL)
  { 
    prev=prev->child;
    while(prev->next!=NULL)
    {
      //  printf("inside the getlastchildloop %s\n",child->name);
      prev=prev->next;
    }
    dirNode=prev;
    dirNode->next=newDirNode;
    //printf("name is : %s\n",dirNode->next->name );
  }
  else
  {
    dirNode->child=newDirNode;
   // printf("name c is : %s\n",dirNode->child->name );
  }
  return 0;
}

static int rmfs_unlink(const char *path)
{
  char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node temp=root,prev=NULL,sib;

  while( token != NULL) 
  {
      prev=temp;sib=NULL;
      temp=temp->child;

      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
          printf("found %s \n",temp->name );
          break;
        }
        sib=temp;      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        return -ENOENT;
      }
      token = strtok(NULL, "/");
  }

  if(sib==NULL)
  {
    prev->child=temp->next;
  }
  else if(sib!=NULL)
  {
    sib->next=temp->next;
  }

  freemalloc(temp);
	return 0;
}

static int rmfs_rmdir(const char *path)
{
  char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node temp=root,prev=NULL,sib;

  while( token != NULL) 
  {
      prev=temp;sib=NULL;
      temp=temp->child;

      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
         // printf("found %s \n",temp->name );
          break;
        }
        sib=temp;      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        return -ENOENT;
      }
      token = strtok(NULL, "/");
  }

  if(temp->type==Ndir && temp->child != NULL)
  {
      return -ENOTEMPTY;
  }

  if(sib==NULL)
  {
    prev->child=temp->next;
  }
  else if(sib!=NULL)
  {
    sib->next=temp->next;
  }

  freemalloc(temp);
  return 0;
}

static int rmfs_access(const char *path, int mask)
{
	Node dirNode=NULL;
    int errChk=directory_lookup(path,&dirNode,0);
    if(errChk==-1)
    	return -ENOENT;
	return 0;
}

static int rmfs_truncate(const char *path, off_t size)
{
  char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node dirNode=root;

  while( token != NULL) 
  {
    dirNode=dirNode->child;
    while(dirNode!=NULL) {
      if (strcmp(token, dirNode->name) == 0) {
          //printf("found %s \n",dirNode->name );
          break;
      }      
        dirNode=dirNode->next; 
    }
    if(dirNode==NULL)
    {
      break;
    }
    token = strtok(NULL, "/");
  }

  if(dirNode==NULL)
    return -ENOENT;

  if(dirNode->type==Ndir)
    	return -EISDIR;

  if(dirNode->data==NULL)
    	return 0;

    malloc_counter-=dirNode->len;
    free(dirNode->data);
    dirNode->data=NULL;
    //memset(dirNode->data,'\0',strlen(dirNode->data));
	return 0;
}

static int rmfs_ftruncate(const char *path, off_t size,struct fuse_file_info *fi)
{
	return 0;
}

static int rmfs_chmod(const char *path, mode_t t)
{
	return 0;
}

static int rmfs_chown(const char *path,uid_t uid, gid_t gid)
{
	return 0;
}

static int rmfs_utimens(const char* path, const struct timespec ts[2])
{
	return 0;
}

static int rmfs_write(const char *path, const char *buf, size_t size,
off_t offset, struct fuse_file_info *fi)
{
	int len=0;
	int newSize=0;
   Node dirNode=root;

  char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");

  while( token != NULL) 
  {
    dirNode=dirNode->child;
    while(dirNode!=NULL) {
      if (strcmp(token, dirNode->name) == 0) {
          //printf("found %s \n",dirNode->name );
          break;
      }      
        dirNode=dirNode->next; 
    }
    if(dirNode==NULL)
    {
      break;
    }
    token = strtok(NULL, "/");
  }

  //dirNode=buffNode;

  if(dirNode==NULL)
    return -ENOENT;
  else if(dirNode->type==Ndir)
    	return -EISDIR;
  else if(buf==NULL)
    	return 0;

  if(dirNode->data!=NULL)
    	len=dirNode->len;
  int mlc_chk=0;
	if (offset + size > len) {
		newSize=offset + size;
		if(len==0){
			mlc_chk=ckmalloc_w(newSize+1,&(dirNode->data));
      dirNode->len=newSize+1;
      if(mlc_chk!=0)
        return mlc_chk;
		}
		else{

       if(malloc_limit < (malloc_counter+(newSize-len)+1))
       {
          return -ENOSPC;
       }
			char *p=realloc(dirNode->data,newSize+1);
      if(p==NULL)
        return errno;
      dirNode->data=p;
			dirNode->data[newSize]='\0';
      dirNode->len=newSize+1;
      malloc_counter+=(newSize-len+1);
		}		
	}
	//printf("dir data :%s\n",dirNode->data);
	strncpy(dirNode->data + offset,buf, size);
	//printf(">>>> after dir data :%s\n",dirNode->data);
	return size;
}

static int rmfs_rename(const char *path,const char *path1)
{
  Node dirNode=NULL;
  char b[FILENAME_SIZE];
  memset(b,'\0',FILENAME_SIZE);
  getFileName(path1,b);

  int errChk=directory_lookup(path,&dirNode,0);
  if(errChk!=0) { 
      return -ENOENT; }
  strncpy(dirNode->name,b,sizeof(b));
  return 0;
}

static struct fuse_operations rmfs_oper = {
	.getattr	= rmfs_getattr,
	.access		= rmfs_access,
	.readdir	= rmfs_readdir,
	.open		= rmfs_open,
	.read		= rmfs_read,
	.mkdir		= rmfs_mkdir,
	.mknod		= rmfs_mknod,
	.flush 		= rmfs_flush,
	.create     = rmfs_create,
	.truncate   = rmfs_truncate,
	.ftruncate  = rmfs_ftruncate,
	.chmod      = rmfs_chmod,
	.chown      = rmfs_chown,
	.utimens    = rmfs_utimens,
	.unlink     = rmfs_unlink,
	.rmdir      = rmfs_rmdir,
	.write 		= rmfs_write,
  .rename   = rmfs_rename,
};


int  makeSamplefile()
{
    int mlc_chk=0;
    strncpy(root->name,"/",strlen("/"));
    root->type=Ndir;
    root->next=NULL;

    Node first;
    mlc_chk=ckmalloc(sizeof(*root),&first);
    if(mlc_chk!=0)
      return mlc_chk;
    strncpy(first->name,"rmfs",strlen("rmfs"));
    first->type=Nfile;
    first->len=sizeof(char)*30;
    first->data=malloc(sizeof(char)*30);
    strncpy(first->data,"This is rmfs\n",strlen("This is rmfs\n"));

    Node sec;
    mlc_chk=ckmalloc(sizeof(*root),&sec);
    if(mlc_chk!=0)
      return ENOMEM;
    strncpy(sec->name,"mello",strlen("mello"));
    sec->type=Nfile;
    sec->len=sizeof(char)*50;
    sec->data=malloc(sizeof(char)*50);
    strncpy(sec->data,"This is mello\n",strlen("This is mello\n"));

    Node third;
    mlc_chk=ckmalloc(sizeof(*root),&third);
    if(mlc_chk!=0)
      return ENOMEM;
    strncpy(third->name,"extra",strlen("extra"));
    third->type=Ndir;

    Node third_ch;
    mlc_chk=ckmalloc(sizeof(*root),&third_ch);
    if(mlc_chk!=0)
      return ENOMEM;
    strncpy(third_ch->name,"file1",strlen("file1"));
    third_ch->type=Nfile;
    third_ch->len=sizeof(char)*30;
    third_ch->data=malloc(sizeof(char)*30);
    strncpy(third_ch->data,"This is file1\n",strlen("This is file1\n"));

    root->child=first;
    first->next=sec;
    first->child=NULL;
    sec->next=third;
    sec->child=NULL;
    third->child=third_ch;
    third_ch->next=NULL;

    return 0;
}

void writeToFile(FILE **p,char pre[FULLPATHNAME],Node t)
{

  if(t==NULL)
    return;

   //printf(" pre : %s node name : %s\n",pre,t->name );
   char finalname[FULLPATHNAME];
   memset(finalname,'\0',FULLPATHNAME);
    //strcat(prefix, src);
   strncpy(finalname, pre,strlen(pre));
   if(strcmp(pre,"/")!=0)
    strcat(finalname, "/");
   strcat(finalname, t->name);

  //printf("inside the writeToFile <%s>\n",finalname);
    if(t->type==Nfile) {
     fprintf(*p, "%d|%s|%ld|\n",0,finalname,(t->data==NULL ? strlen("EMPTY\n"):strlen(t->data)));
     fprintf(*p, "%s",(t->data==NULL ? "EMPTY\n" : t->data));
   }
   else
   {
     fprintf(*p, "%d|%s|\n",1,finalname);
   }

   if(t->child != NULL)
    writeToFile(p,finalname,t->child);

   if(t->next != NULL)
    writeToFile(p,pre,t->next);

   return;
}


void usePersistentFile(FILE **fp)
{

  while (1) 
  {
    ssize_t read;
    char * line = NULL;
    size_t len = 0;
    mode_t mode=0;
    int status=0;
    struct fuse_file_info *fi=NULL;

    if((read = getline(&line, &len, *fp)) == -1)
    {
       break;
    }

    //printf("Retrieved line of length %zu :\n", read);
    //printf("%s", line);
    long numr;
    char *token=strtok(line, "|");           
    int isfile=atoi(token);
    //printf(" isfile :%d\n",isfile );     
    token = strtok(NULL, "|");
    //printf("filename <%s>\n",token);
    if(isfile ==0)
    {
        char fullfilepath[1000];
        memset(fullfilepath,'\0',1000);
        strncpy(fullfilepath,token,strlen(token));
      //  printf("fullfilepath <%s>\n",fullfilepath);
        token = strtok(NULL, "|");
        long filelen=atol(token);
      //  printf("filelen <%ld>\n",filelen);
        char* buffer=malloc(filelen);
        memset(buffer,'\0',filelen);
        if((numr=fread(buffer,1,filelen,*fp))!=filelen){
            if(ferror(*fp)!=0){
                fprintf(stderr,"read file error.\n");
                exit(1);
            }
        }
       // printf(" buffer :<%s>\n",buffer );
        status=rmfs_create(fullfilepath, mode,fi);
        if(status !=0)
          break;
        if(strcmp(buffer,"EMPTY\n")!=0)
	{
          status=rmfs_write(fullfilepath, buffer, filelen,0, fi);
      	  if(status !=filelen)
            break;
	}
        free(buffer);
    }
    else
    {
      status=rmfs_mkdir(token,mode);
      if(status !=0)
          break;
    }

    if (line)
       free(line);
  }
}

int main(int argc, char *argv[])
{
  int eflag=0,pstr=0;
  FILE * fp;
  if(argc < 3 || argc >4)
  {
    printf("Usage : ./ramdisk <mount_point> <size> <Restore Filepath> \n");
    exit(-1);
  }

  malloc_limit=atoi(argv[2]);
  malloc_limit=malloc_limit*1024*1024; 
  argv[2]="-d";

  if(argc == 4)
    pstr=1;
  argc=2;
  //defining the root element
  int mlc_chk=ckmalloc(sizeof(*root),&root);
  if(mlc_chk!=0)
      return mlc_chk;
  strncpy(root->name,"/",strlen("/"));
  root->type=Ndir;
  root->next=root->child=NULL;

  if(pstr == 1)
  {    
    fp = fopen (argv[3], "a+");
    if (fp == NULL)
      exit(EXIT_FAILURE);
 
    usePersistentFile(&fp);
    fclose(fp);
  }
  //  eflag=makeSamplefile();
   eflag=fuse_main(argc, argv, &rmfs_oper, NULL);

  if(pstr == 1)
  {
    char prefix[1000]="/";
    fp = fopen (argv[3], "w+");
    if (fp == NULL)
       exit(EXIT_FAILURE);

    writeToFile(&fp,prefix,root->child);
   
    fclose(fp);
  }

  return eflag;
}

