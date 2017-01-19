//=====================================================================
//
// test.cpp - kcplib ��������
//
// ˵����
// gcc test.cpp -o test -lstdc++
//
//=====================================================================

#include <stdio.h>
#include <stdlib.h>

#include "test.h"
#include "ikcp.c"


// ģ������
LatencySimulator *vnet;

// ģ�����磺ģ�ⷢ��һ�� udp��
int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	union { int id; void *ptr; } parameter;
	parameter.ptr = user;
	vnet->send(parameter.id, buf, len);
	return 0;
}

// ��������
void test(int mode)
{
	// ����ģ�����磺������10%��Rtt 60ms~125ms
	vnet = new LatencySimulator(10, 60, 125);

	// ���������˵�� kcplib���󣬵�һ������ conv�ǻỰ��ţ�ͬһ���Ự��Ҫ��ͬ
	// ���һ���� user�������������ݱ�ʶ
	ikcpcb *kcp1 = ikcp_create(0x11223344, (void*)0);
	ikcpcb *kcp2 = ikcp_create(0x11223344, (void*)1);

	// ����kcplib���²����������Ϊ udp_output��ģ��udp�����������
	kcp1->output = udp_output;
	kcp2->output = udp_output;

	IUINT32 current = iclock();
	IUINT32 slap = current + 20;
	IUINT32 index = 0;
	IUINT32 next = 0;
	IINT64 sumrtt = 0;
	int count = 0;
	int maxrtt = 0;

	// ���ô��ڴ�С��ƽ���ӳ�200ms��ÿ20ms����һ������
	// �����ǵ������ط�����������շ�����Ϊ128
	ikcp_wndsize(kcp1, 128, 128);
	ikcp_wndsize(kcp2, 128, 128);

	// �жϲ���������ģʽ
	if (mode == 0) {
		// Ĭ��ģʽ
		ikcp_nodelay(kcp1, 0, 10, 0, 0);
		ikcp_nodelay(kcp2, 0, 10, 0, 0);
	}
	else if (mode == 1) {
		// ��ͨģʽ���ر����ص�
		ikcp_nodelay(kcp1, 0, 10, 0, 1);
		ikcp_nodelay(kcp2, 0, 10, 0, 1);
	}	else {
		// ��������ģʽ
		// �ڶ������� nodelay-�����Ժ����ɳ�����ٽ�����
		// ���������� intervalΪ�ڲ�����ʱ�ӣ�Ĭ������Ϊ 10ms
		// ���ĸ����� resendΪ�����ش�ָ�꣬����Ϊ2
		// ��������� Ϊ�Ƿ���ó������أ������ֹ
		ikcp_nodelay(kcp1, 1, 10, 2, 1);
		ikcp_nodelay(kcp2, 1, 10, 2, 1);
		kcp1->rx_minrto = 10;
		kcp1->fastresend = 1;
	}


	char buffer[2000];
	int hr;

	IUINT32 ts1 = iclock();

	while (1) {
		isleep(1);
		current = iclock();
		ikcp_update(kcp1, iclock());
		ikcp_update(kcp2, iclock());

		// ÿ�� 20ms��kcp1��������
		for (; current >= slap; slap += 20) {
			((IUINT32*)buffer)[0] = index++;
			((IUINT32*)buffer)[1] = current;

			// �����ϲ�Э���
			ikcp_send(kcp1, buffer, 8);
		}

		// �����������磺����Ƿ���udp����p1->p2
		while (1) {
			hr = vnet->recv(1, buffer, 2000);
			if (hr < 0) break;
			// ��� p2�յ�udp������Ϊ�²�Э�����뵽kcp2
			ikcp_input(kcp2, buffer, hr);
		}

		// �����������磺����Ƿ���udp����p2->p1
		while (1) {
			hr = vnet->recv(0, buffer, 2000);
			if (hr < 0) break;
			// ��� p1�յ�udp������Ϊ�²�Э�����뵽kcp1
			ikcp_input(kcp1, buffer, hr);
		}

		// kcp2���յ��κΰ������ػ�ȥ
		while (1) {
			hr = ikcp_recv(kcp2, buffer, 10);
			// û���յ������˳�
			if (hr < 0) break;
			// ����յ����ͻ���
			ikcp_send(kcp2, buffer, hr);
		}

		// kcp1�յ�kcp2�Ļ�������
		while (1) {
			hr = ikcp_recv(kcp1, buffer, 10);
			// û���յ������˳�
			if (hr < 0) break;
			IUINT32 sn = *(IUINT32*)(buffer + 0);
			IUINT32 ts = *(IUINT32*)(buffer + 4);
			IUINT32 rtt = current - ts;
			
			if (sn != next) {
				// ����յ��İ�������
				printf("ERROR sn %d<->%d\n", (int)count, (int)next);
				return;
			}

			next++;
			sumrtt += rtt;
			count++;
			if (rtt > (IUINT32)maxrtt) maxrtt = rtt;

			printf("[RECV] mode=%d sn=%d rtt=%d\n", mode, (int)sn, (int)rtt);
		}
		if (next > 1000) break;
	}

	ts1 = iclock() - ts1;

	ikcp_release(kcp1);
	ikcp_release(kcp2);

	const char *names[3] = { "default", "normal", "fast" };
	printf("%s mode result (%dms):\n", names[mode], (int)ts1);
	printf("avgrtt=%d maxrtt=%d tx=%d\n", (int)(sumrtt / count), (int)maxrtt, (int)vnet->tx1);
	printf("press enter to next ...\n");
	char ch; scanf("%c", &ch);
}

int main()
{
	test(0);	// Ĭ��ģʽ������ TCP������ģʽ���޿����ش�����������
	test(1);	// ��ͨģʽ���ر����ص�
	test(2);	// ����ģʽ�����п��ض��򿪣��ҹر�����
	return 0;
}

/*
default mode result (20917ms):
avgrtt=740 maxrtt=1507

normal mode result (20131ms):
avgrtt=156 maxrtt=571

fast mode result (20207ms):
avgrtt=138 maxrtt=392
*/

