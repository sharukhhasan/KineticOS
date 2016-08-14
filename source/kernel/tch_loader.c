/**
 *
 * Created by Sharukh Hasan on 8/11/16.
 * Copyright © 2016 HyperCryptic Solutions, LLC. All rights reserved.
 *
 */




#include "tch_loader.h"

struct proc_header default_prochdr = { 0 };


struct proc_header* tch_procLoadFromFile(struct tch_file* filp,const char* argv[]){
	struct proc_header* header = (struct proc_header*) kmalloc(sizeof(struct proc_header));
}

struct proc_header* tch_procExec(const char* path,const char* argv[]){

}
