// アーキ非依存な単純数学関数（ホストテストでも利用）
// 名前衝突（libc の abs）を避けるため kfs_abs にリネーム
int kfs_abs(int x)
{
	if (x < 0)
		return -x;
	return x;
}
