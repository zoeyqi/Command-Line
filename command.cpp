#include <fstream>
#include <iostream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
#include <vector>
#include <limits.h>
#include <stack>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
using namespace std;


vector<vector<string> > &parse(string comline, vector<vector<string> > &pipp){
  vector<string> pnode;
  string token;
  for(string::iterator it = comline.begin(); it!= comline.end(); ++it){
    switch (*it) {
      case '\\':
        token += *(++it);
        break;
      case '|':
        if (!token.empty()){
	  pnode.push_back(token);
	  token.clear();
        }
        pipp.push_back(pnode);
        pnode.clear();
        break;
      case ' ':
        if(!token.empty()){
	  pnode.push_back(token);
	  token.clear();
        }
        break;
      default:
        token += *it;
    }
  }
  if(!token.empty()){
    pnode.push_back(token);
  }
  pipp.push_back(pnode);
  return pipp;
}


int main(){
  //build path stack
  stack<string> pathStack;
  
  while (true){
    //get the current directory 
    char buff[PATH_MAX];
    char * cwd = getcwd(buff,PATH_MAX);
    string cwdstr = cwd;
    //prompt
    int inodeindex = 0;
    string comline;
    cout<<"myshell:"<<cwd<<" $ ";
    getline(cin, comline);
    cout<<"input:"<<comline<<endl;
    //path
    char cwdpath[PATH_MAX];
    if(getcwd(cwdpath, PATH_MAX)==NULL){
      perror("error");
      return EXIT_FAILURE;
    }
    //when encounter EOF
    if(cin.eof()){
      cout<<"encounter EOF"<<endl;
      break;
    }
    
    vector<vector<string> > pipenode;
    //after paring
    parse(comline, pipenode);
    //the first pipenode is a vector of string 
    vector<string> wordVect = pipenode[0];
    
    //when there is no input
    if(wordVect.empty())continue;
    
    //when user type "exit"
    if(wordVect[0] == "exit"){
      cout<<"exit program"<<endl;
      break;
    }
    
    //------------------------Step 3-------------------------//
    if( wordVect[0].find("pushd") != string::npos||
	wordVect[0].find("popd") != string::npos ||
	wordVect[0].find("dirstack") != string::npos ||
	wordVect[0].find("cd") != string::npos ){
      if(wordVect[0] == "cd"){
	//change to home directory
	if(wordVect[1] == "~" || wordVect.size() == 1){
	  char * homedir = getenv("HOME");
	  cout<<"HOME DIR"<<homedir<<endl;
	  chdir(homedir);
	  continue;
	}
	//change to the parent directory 
	if(wordVect[1] == ".."){
	  string nwcwd = cwdstr.substr(0, cwdstr.find_last_of("/"));
	  chdir(nwcwd.c_str());
	  continue;
	} 
	//cd. keep here
	if(wordVect[1] == "."){
	  cout<<"stay at the current directory"<<endl;
	  continue;
	}
	//cd path to directory :go that directory
	else{
	  char actualpath[PATH_MAX];
	  char* ptr = realpath(wordVect[1].c_str(), actualpath);
	  cout<<"the path to the directory is:"<<ptr<<endl;
	  int cdirn  = chdir(ptr);
	  cout<<"check if change directory succeed or not:"<<endl;
	  if(cdirn == 0){cout<<"changed directory successfully"<<endl;}
	  if(cdirn != 0){
	    cout<<"cant's change directory"<<endl;
	  }
	}
      }
      if(wordVect[0] ==  "pushd"){
	if(wordVect.size() > 1){//there is a new path of directory
	//change dir 
	  cout<<"the new path is:"<<wordVect[1].c_str()<<endl;
	  int cdir = chdir(wordVect[1].c_str());
	  cout<<"check if change directory succeed or not "<<cdir<<endl;
	  if(cdir == 0){//succeed
	    //push the old directory onto the stack
	    
	    cout<<"the old path is "<<cwdstr<<endl;
	    pathStack.push(cwdstr);
	  }
	  else{
	    cout<<"cant's change directory"<<endl;
	  }
	}
      }
      if(wordVect[0] ==  "popd" ){
	if( !pathStack.empty()){
	  //change the current dir to that one
	  chdir(( pathStack.top() ).c_str());
	  //pop 
	  pathStack.pop();
	}
	else{
	  cout<<"the stack is empty, cannot pop"<<endl;
	}
      }

      if(wordVect[0] ==  "dirstack"){
	stack<string> pathS;
	while(!pathStack.empty()){
	  pathS.push(pathStack.top());
	  pathStack.pop();
	}
	while(!pathS.empty()){
	  cout<<pathS.top()<<endl;
	  pathStack.push(pathS.top());
	  pathS.pop();
	}
      }
      continue;
    }
    //-----------step 4-----------------//
    int numNode = pipenode.size();
    int numPipe = (pipenode.size()) - 1;
    //create  pipe
    
    int n = 2 * numPipe;
    int *pfd = new int[n];
    for(int h=0; h < numPipe; h++){
      pipe(pfd + (2*h));
    }
   
    
    
    
    for(int j = 0; j < numNode; j++){//loop through pipe
      //fork
      pid_t pid = fork();
      if (pid == 0){
	if(numPipe>0){
	  if(inodeindex == 0){//the first pipe node
	    dup2(pfd[1],1);
	  }
	  if(inodeindex <numPipe){
	    dup2(pfd[inodeindex * 2 -2], 0);
	    dup2(pfd[inodeindex * 2 +1], 1);
	  }
	  if(inodeindex == numPipe){
	    dup2(pfd[inodeindex *2 - 2], 0);//the last pipe node
	  }
	  for(int k = 0; k < (2*numPipe); k++){
	  close(pfd[k]);
	  }
	}
	int size = pipenode[j].size();
      
      
      //deal with redirection and get the argument list
	vector<string> arglist;
	int innerct = 0;
	for(vector<string>::iterator it1 = pipenode[j].begin(); it1 != pipenode[j].end(); ++it1){
	  if(*it1 == "<"){
	    if(size-1 > innerct){
	      int in = open((*(++it1)).c_str(), O_RDONLY);
	      if(in == -1){cout<<"error in redirection: cannot open input file"<<endl; exit(EXIT_FAILURE);}
	      int d = dup2(in, 0);
	      if(d == -1){cout<<"error in redirection:cannot dup2"<<endl; exit(EXIT_FAILURE);}
	      close(in);
	    }
	    continue;
	  }
	  
	  if(*it1 == ">"){
	  if(size-1 > innerct){
	    int out = open((*(++it1)).c_str(), O_WRONLY| O_CREAT ,S_IRUSR| S_IRGRP| S_IWGRP| S_IWUSR);
	    if(out == -1){perror("error in redirection:cannot open output file"); exit (EXIT_FAILURE);}
	    int d = dup2(out, 1);
	    if(d == -1){perror("error in redirection: cannot dup2"); exit (EXIT_FAILURE);}
	    close(out);
	  }
	  continue;
	  }
	  
	  if(*it1 == "2>"){
	    if(size-1 > innerct){
	      int errout = open((*(++it1)).c_str(), O_WRONLY | O_CREAT, S_IRUSR| S_IRGRP| S_IWGRP| S_IWUSR);
	      if(errout == -1){perror("error in redirection:cannot open error out put file"); exit (EXIT_FAILURE);}
	    int d = dup2(errout, 2);
	    if(d == -1){perror("error in redirection:cannot dup2"); exit (EXIT_FAILURE);}
	    close(errout);
	    }
	    continue;
	  }
	  arglist.push_back(*it1);
	  innerct++;
	}
	char ** arg = new char *[innerct+1];
	for(size_t r=0; r<arglist.size(); r++){
	  arg[r] = strdup(arglist[r].c_str());
	}
	arg[innerct] = NULL;
	execvp(arg[0], arg);
      }
      inodeindex++;
    }
    for(int k = 0; k < 2*numPipe; k++){
      close(pfd[k]);
    }
    
    pid_t w;
    int status;
    for(int v = 0; v < numNode; v++){
      w = wait(&status);
      
      if(w == -1) {
	perror("wait");
	exit (EXIT_FAILURE);
      }
      if (WIFEXITED(status)){
	printf("program exited with  status %d\n", WEXITSTATUS(status));
	}
      else if (WIFSIGNALED(status)){
	printf("program was killed by signal %d\n", WTERMSIG(status));
      }
    }
    delete[] pfd;
  }
  return(EXIT_SUCCESS);
}

