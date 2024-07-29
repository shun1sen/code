#include<vector>
#include<iostream>
using namespace std;

int main()
{
    int n[3]={1,2,3};
    vector<int> a(n,n+3);
    int b,c;
    int i=2,j=2;
    b=a[i--];
    c=a[--j];
    cout<<b<<'\t'<<c;
}