int n, m;
int pa[1001];
void makeset(int n)
{
	for (int i = 1; i <= n; ++i)
		pa[i] = i;
}
int findset(int x)
{
	if (x != pa[x])
		pa[x] = findset(pa[x]);
	return  pa[x];
}
void unionset(int x, int y)
{
	x = findset(x);
	y = findset(y);
	if (x == y)
		return;
	pa[x] = y;
}
int main()
{
	int n=5, m=2, a, b, i;
	makeset(n);
	for (int j = 0; j<m; j++)
	{
		a = 1;
        b = 3;
		a = findset(a);
		b = findset(b);
		unionset(a, b);
	}
	int cnt = 0;
	int result = findset(1);
	for (int k = 2; k <= n; k++)
	{
		if (findset(k) == result)
			++cnt;
	}
	return 0;
}