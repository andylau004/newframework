
#pragma once




struct BinaryTreeNode
{
    int                    m_nValue;
    BinaryTreeNode*        m_pLeft;
    BinaryTreeNode*        m_pRight;
};



extern BinaryTreeNode* CreateBinaryTreeNode(int value);
extern void ConnectTreeNodes(BinaryTreeNode* pParent, BinaryTreeNode* pLeft, BinaryTreeNode* pRight);
extern void PrintTreeNode(const BinaryTreeNode* pNode);
extern void PrintTree(const BinaryTreeNode* pRoot);
extern void DestroyTree(BinaryTreeNode* pRoot);


//__declspec( dllexport ) BinaryTreeNode* CreateBinaryTreeNode(int value);
//__declspec( dllexport ) void ConnectTreeNodes(BinaryTreeNode* pParent, BinaryTreeNode* pLeft, BinaryTreeNode* pRight);
//__declspec( dllexport ) void PrintTreeNode(const BinaryTreeNode* pNode);
//__declspec( dllexport ) void PrintTree(const BinaryTreeNode* pRoot);
//__declspec( dllexport ) void DestroyTree(BinaryTreeNode* pRoot);


