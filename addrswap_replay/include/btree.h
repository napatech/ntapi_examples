/*
 * yaml_parse.h
 *
 *  Created on: Jan 29, 2021
 *      Author: napatech
 */

#ifndef INCLUDE_BTREE_H_
#define INCLUDE_BTREE_H_

struct bin_tree {
int key;
int data;
struct bin_tree * right, * left;
};
typedef struct bin_tree node;


void insert(node ** tree, int key, int data);

void print_preorder(node * tree);
void print_inorder(node * tree);
void print_postorder(node * tree);

void deltree(node * tree);
node* search(node ** tree, int val);



#endif /* INCLUDE_BTREE_H_ */
