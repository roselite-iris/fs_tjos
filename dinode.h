class DiskInode
{
public:
	DiskInode() {;};
	~DiskInode() {;};
public:
	unsigned int d_mode;	/* ״̬�ı�־λ�������enum INodeFlag */
	int		d_nlink;		/* �ļ���������������ļ���Ŀ¼���в�ͬ·���������� ������*/
	
	short	d_uid;			/* �ļ������ߵ��û���ʶ�� */
	short	d_gid;			/* �ļ������ߵ����ʶ�� */
	
	int		d_size;			/* �ļ���С���ֽ�Ϊ��λ */
	int		d_addr[10];		/* �����ļ��߼���ź�������ת���Ļ��������� ������*/
	
	int		d_atime;		/* ������ʱ�� */
	int		d_mtime;		/* ����޸�ʱ�� */
};