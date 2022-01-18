
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "include/btree.h"


//=====================================================================
void insert(node ** tree, int key, int data)
{
    node *temp = NULL;
    if(!(*tree))
    {
        temp = (node *)malloc(sizeof(node));
        temp->left = temp->right = NULL;
        temp->key = key;
        temp->data = data;
        *tree = temp;
        return;
    }

    if(key < (*tree)->key)
    {
        insert(&(*tree)->left, key, data);
    }
    else if(key > (*tree)->key)
    {
        insert(&(*tree)->right, key, data);
    }
}

void print_preorder(node * tree)
{
    if (tree)
    {
        printf("0x%08x --> 0x%08x\n",tree->key, tree->data);
        print_preorder(tree->left);
        print_preorder(tree->right);
    }
}

void print_inorder(node * tree)
{
    if (tree)
    {
        print_inorder(tree->left);
        printf("0x%08x --> 0x%08x\n",tree->key, tree->data);
        print_inorder(tree->right);
    }
}

void print_postorder(node * tree)
{
    if (tree)
    {
        print_postorder(tree->left);
        print_postorder(tree->right);
        printf("0x%08x --> 0x%08x\n",tree->key, tree->data);
    }
}

void deltree(node * tree)
{
    if (tree)
    {
        deltree(tree->left);
        deltree(tree->right);
        free(tree);
    }
}

node* search(node ** tree, int val)
{
    if(!(*tree))
    {
        return NULL;
    }

    if(val < (*tree)->key)
    {
        return search(&((*tree)->left), val);
    }
    else if(val > (*tree)->key)
    {
        return search(&((*tree)->right), val);
    }
    else if(val == (*tree)->key)
    {
    	return *tree;
    }

    printf("******  SHOULD NOT GET HERE ******\n");
    return NULL;
}
