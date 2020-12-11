int n, m;
int arr[1001];
int findset(int x)
{
	if (x != arr[x])
		arr[x] = findset(arr[x]);
	return  arr[x];
}
void unionset(int x, int y)
{
	x = findset(x);
	y = findset(y);
	if (x == y)
		return;
	arr[x] = y;
}
int main()
{
	int n=5, m=2, a, b;
	for (int i = 1; i <= n; ++i)
		arr[i] = i;
	for (int j = 0; j<m; ++j)
	{
		a = 1;
        b = 3;
		a = findset(a);
		b = findset(b);
		unionset(a, b);
	}
	int result = findset(1);
    int cnt = 0;
	for (int k = 2; k <= n; ++k)
	{
		if (findset(k) == result)
			cnt += 1;
	}
	return 0;
}