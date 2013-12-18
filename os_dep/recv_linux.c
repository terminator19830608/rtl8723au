/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _RECV_OSDEP_C_

#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>

#include <wifi.h>
#include <recv_osdep.h>

#include <osdep_intf.h>
#include <ethernet.h>

#include <usb_ops.h>

//init os related resource in struct recv_priv
int rtw_os_recv_resource_init(struct recv_priv *precvpriv, struct rtw_adapter *padapter)
{
	int	res=_SUCCESS;

	return res;
}

//alloc os related resource in union recv_frame
int rtw_os_recv_resource_alloc(struct rtw_adapter *padapter, union recv_frame *precvframe)
{
	int	res=_SUCCESS;

	precvframe->u.hdr.pkt_newalloc = precvframe->u.hdr.pkt = NULL;

	return res;

}

//free os related resource in union recv_frame
void rtw_os_recv_resource_free(struct recv_priv *precvpriv)
{

}


//alloc os related resource in struct recv_buf
int rtw_os_recvbuf_resource_alloc(struct rtw_adapter *padapter, struct recv_buf *precvbuf)
{
	int res=_SUCCESS;

	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct usb_device	*pusbd = pdvobjpriv->pusbdev;

	precvbuf->irp_pending = false;
	precvbuf->purb = usb_alloc_urb(0, GFP_KERNEL);
	if(precvbuf->purb == NULL){
		res = _FAIL;
	}

	precvbuf->pskb = NULL;

	precvbuf->reuse = false;

	precvbuf->pallocated_buf  = precvbuf->pbuf = NULL;

	precvbuf->pdata = precvbuf->phead = precvbuf->ptail = precvbuf->pend = NULL;

	precvbuf->transfer_len = 0;

	precvbuf->len = 0;
	return res;
}

//free os related resource in struct recv_buf
int rtw_os_recvbuf_resource_free(struct rtw_adapter *padapter, struct recv_buf *precvbuf)
{
	int ret = _SUCCESS;

	if(precvbuf->purb)
		usb_free_urb(precvbuf->purb);

	if(precvbuf->pskb)
		dev_kfree_skb_any(precvbuf->pskb);

	return ret;
}

void rtw_handle_tkip_mic_err(struct rtw_adapter *padapter,u8 bgroup)
{
	enum nl80211_key_type key_type;
	union iwreq_data wrqu;
	struct iw_michaelmicfailure    ev;
	struct mlme_priv*              pmlmepriv  = &padapter->mlmepriv;
	struct security_priv	*psecuritypriv = &padapter->securitypriv;
	u32 cur_time = 0;

	if( psecuritypriv->last_mic_err_time == 0 )
	{
		psecuritypriv->last_mic_err_time = rtw_get_current_time();
	}
	else
	{
		cur_time = rtw_get_current_time();

		if( cur_time - psecuritypriv->last_mic_err_time < 60*HZ )
		{
			psecuritypriv->btkip_countermeasure = true;
			psecuritypriv->last_mic_err_time = 0;
			psecuritypriv->btkip_countermeasure_time = cur_time;
		}
		else
		{
			psecuritypriv->last_mic_err_time = rtw_get_current_time();
		}
	}

	if (bgroup) {
		key_type |= NL80211_KEYTYPE_GROUP;
	} else {
		key_type |= NL80211_KEYTYPE_PAIRWISE;
	}

	cfg80211_michael_mic_failure(padapter->pnetdev, (u8 *)&pmlmepriv->assoc_bssid[ 0 ], key_type, -1,
		NULL, GFP_ATOMIC);

	memset(&ev, 0x00, sizeof(ev));
	if ( bgroup )
	{
	    ev.flags |= IW_MICFAILURE_GROUP;
	}
	else
	{
	    ev.flags |= IW_MICFAILURE_PAIRWISE;
	}

	ev.src_addr.sa_family = ARPHRD_ETHER;
	memcpy(ev.src_addr.sa_data, &pmlmepriv->assoc_bssid[ 0 ], ETH_ALEN );

	memset(&wrqu, 0x00, sizeof(wrqu));
	wrqu.data.length = sizeof( ev );
}

void rtw_hostapd_mlme_rx(struct rtw_adapter *padapter, union recv_frame *precv_frame)
{
#ifdef CONFIG_HOSTAPD_MLME
	struct sk_buff *skb;
	struct hostapd_priv *phostapdpriv  = padapter->phostapdpriv;
	struct net_device *pmgnt_netdev = phostapdpriv->pmgnt_netdev;

	RT_TRACE(_module_recv_osdep_c_, _drv_info_, ("+rtw_hostapd_mlme_rx\n"));

	skb = precv_frame->u.hdr.pkt;

	if (skb == NULL)
		return;

	skb->data = precv_frame->u.hdr.rx_data;
	skb->tail = precv_frame->u.hdr.rx_tail;
	skb->len = precv_frame->u.hdr.len;

	//pskb_copy = skb_copy(skb, GFP_ATOMIC);
//	if(skb == NULL) goto _exit;

	skb->dev = pmgnt_netdev;
	skb->ip_summed = CHECKSUM_NONE;
	skb->pkt_type = PACKET_OTHERHOST;
	//skb->protocol = __constant_htons(0x0019); /*ETH_P_80211_RAW*/
	skb->protocol = __constant_htons(0x0003); /*ETH_P_80211_RAW*/

	//DBG_8723A("(1)data=0x%x, head=0x%x, tail=0x%x, mac_header=0x%x, len=%d\n", skb->data, skb->head, skb->tail, skb->mac_header, skb->len);

	//skb->mac.raw = skb->data;
	skb_reset_mac_header(skb);

       //skb_pull(skb, 24);
       memset(skb->cb, 0, sizeof(skb->cb));

	netif_rx(skb);

	precv_frame->u.hdr.pkt = NULL; // set pointer to NULL before rtw_free_recvframe() if call netif_rx()
#endif
}

int rtw_recv_indicatepkt(struct rtw_adapter *padapter, union recv_frame *precv_frame)
{
	struct recv_priv *precvpriv;
	_queue	*pfree_recv_queue;
	struct sk_buff *skb;
	struct mlme_priv*pmlmepriv = &padapter->mlmepriv;

#ifdef CONFIG_BR_EXT
	void *br_port = NULL;
#endif

_func_enter_;

	precvpriv = &(padapter->recvpriv);
	pfree_recv_queue = &(precvpriv->free_recv_queue);

#ifdef CONFIG_DRVEXT_MODULE
	if (drvext_rx_handler(padapter, precv_frame->u.hdr.rx_data, precv_frame->u.hdr.len) == _SUCCESS)
	{
		goto _recv_indicatepkt_drop;
	}
#endif

	skb = precv_frame->u.hdr.pkt;
	if(skb == NULL)
	{
		RT_TRACE(_module_recv_osdep_c_,_drv_err_,("rtw_recv_indicatepkt():skb==NULL something wrong!!!!\n"));
		goto _recv_indicatepkt_drop;
	}

	RT_TRACE(_module_recv_osdep_c_,_drv_info_,("rtw_recv_indicatepkt():skb != NULL !!!\n"));
	RT_TRACE(_module_recv_osdep_c_,_drv_info_,("rtw_recv_indicatepkt():precv_frame->u.hdr.rx_head=%p  precv_frame->hdr.rx_data=%p\n", precv_frame->u.hdr.rx_head, precv_frame->u.hdr.rx_data));
	RT_TRACE(_module_recv_osdep_c_,_drv_info_,("precv_frame->hdr.rx_tail=%p precv_frame->u.hdr.rx_end=%p precv_frame->hdr.len=%d \n", precv_frame->u.hdr.rx_tail, precv_frame->u.hdr.rx_end, precv_frame->u.hdr.len));

	skb->data = precv_frame->u.hdr.rx_data;

	skb_set_tail_pointer(skb, precv_frame->u.hdr.len);

	skb->len = precv_frame->u.hdr.len;

	RT_TRACE(_module_recv_osdep_c_, _drv_info_,
		 ("\n skb->head=%p skb->data=%p skb->tail=%p skb->end=%p skb->len=%d\n",
		 skb->head, skb->data, skb_tail_pointer(skb), skb_end_pointer(skb), skb->len));

	if(check_fwstate(pmlmepriv, WIFI_AP_STATE) == true)
	{
		struct sk_buff *pskb2=NULL;
		struct sta_info *psta = NULL;
		struct sta_priv *pstapriv = &padapter->stapriv;
		struct rx_pkt_attrib *pattrib = &precv_frame->u.hdr.attrib;
		int bmcast = is_multicast_ether_addr(pattrib->dst);

		//DBG_8723A("bmcast=%d\n", bmcast);

		if(memcmp(pattrib->dst, myid(&padapter->eeprompriv), ETH_ALEN))
		{
			//DBG_8723A("not ap psta=%p, addr=%pM\n", psta, pattrib->dst);

			if(bmcast)
			{
				psta = rtw_get_bcmc_stainfo(padapter);
				pskb2 = skb_clone(skb, GFP_ATOMIC);
			} else {
				psta = rtw_get_stainfo(pstapriv, pattrib->dst);
			}

			if(psta)
			{
				struct net_device *pnetdev= (struct net_device*)padapter->pnetdev;

				//DBG_8723A("directly forwarding to the rtw_xmit_entry\n");

				//skb->ip_summed = CHECKSUM_NONE;
				skb->dev = pnetdev;
				skb_set_queue_mapping(skb, rtw_recv_select_queue(skb));

				rtw_xmit_entry(skb, pnetdev);

				if(bmcast)
					skb = pskb2;
				else
					goto _recv_indicatepkt_end;
			}


		}
		else// to APself
		{
			//DBG_8723A("to APSelf\n");
		}
	}


#ifdef CONFIG_BR_EXT

	rcu_read_lock();
	br_port = rcu_dereference(padapter->pnetdev->rx_handler_data);
	rcu_read_unlock();

	if( br_port	&& (check_fwstate(pmlmepriv, WIFI_STATION_STATE|WIFI_ADHOC_STATE) == true) )
		int nat25_handle_frame(struct rtw_adapter *priv, struct sk_buff *skb);

#endif	// CONFIG_BR_EXT

	skb->ip_summed = CHECKSUM_NONE;
	skb->dev = padapter->pnetdev;
	skb->protocol = eth_type_trans(skb, padapter->pnetdev);

	netif_rx(skb);

_recv_indicatepkt_end:

	precv_frame->u.hdr.pkt = NULL; // pointers to NULL before rtw_free_recvframe()

	rtw_free_recvframe(precv_frame, pfree_recv_queue);

	RT_TRACE(_module_recv_osdep_c_,_drv_info_,("\n rtw_recv_indicatepkt :after netif_rx!!!!\n"));

_func_exit_;

        return _SUCCESS;

_recv_indicatepkt_drop:

	 rtw_free_recvframe(precv_frame, pfree_recv_queue);

_func_exit_;
	 return _FAIL;
}

void rtw_os_read_port(struct rtw_adapter *padapter, struct recv_buf *precvbuf)
{
	struct recv_priv *precvpriv = &padapter->recvpriv;

	precvbuf->ref_cnt--;

	//free skb in recv_buf
	dev_kfree_skb_any(precvbuf->pskb);

	precvbuf->pskb = NULL;
	precvbuf->reuse = false;

	if(precvbuf->irp_pending == false)
		rtw_read_port(padapter, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
}

void rtw_init_recv_timer(struct recv_reorder_ctrl *preorder_ctrl)
{
	setup_timer(&preorder_ctrl->reordering_ctrl_timer,
		    rtw_reordering_ctrl_timeout_handler,
		    (unsigned long)preorder_ctrl);
}
