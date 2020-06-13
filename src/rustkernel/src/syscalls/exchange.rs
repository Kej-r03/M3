/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

use base::col::ToString;
use base::errors::Code;
use base::kif::{self, CapRngDesc, CapSel, CapType};
use base::rc::{Rc, Weak};
use base::tcu;
use base::util;

use cap::KObject;
use cap::{ServObject, SessObject};
use com::Service;
use pes::VPE;
use syscalls::{get_request, reply_success, send_reply, SyscError};

fn do_exchange(
    vpe1: &Rc<VPE>,
    vpe2: &Rc<VPE>,
    c1: &kif::CapRngDesc,
    c2: &kif::CapRngDesc,
    obtain: bool,
) -> Result<(), SyscError> {
    let src = if obtain { vpe2 } else { vpe1 };
    let dst = if obtain { vpe1 } else { vpe2 };
    let src_rng = if obtain { c2 } else { c1 };
    let dst_rng = if obtain { c1 } else { c2 };

    if vpe1.id() == vpe2.id() {
        return Err(SyscError::new(
            Code::InvArgs,
            "Cannot exchange with same VPE".to_string(),
        ));
    }
    if c1.cap_type() != c2.cap_type() {
        return Err(SyscError::new(
            Code::InvArgs,
            format!("Cap types differ ({} vs {})", c1.cap_type(), c2.cap_type()),
        ));
    }
    if (obtain && c2.count() > c1.count()) || (!obtain && c2.count() != c1.count()) {
        return Err(SyscError::new(
            Code::InvArgs,
            format!("Cap counts differ ({} vs {})", c2.count(), c1.count()),
        ));
    }
    if !dst.obj_caps().borrow().range_unused(dst_rng) {
        return Err(SyscError::new(
            Code::InvArgs,
            "Destination selectors already in use".to_string(),
        ));
    }

    for i in 0..c2.count() {
        let src_sel = src_rng.start() + i;
        let dst_sel = dst_rng.start() + i;
        let mut obj_caps_ref = src.obj_caps().borrow_mut();
        let src_cap = obj_caps_ref.get_mut(src_sel);
        src_cap.map(|c| dst.obj_caps().borrow_mut().obtain(dst_sel, c, true));
    }

    Ok(())
}

#[inline(never)]
pub fn exchange(vpe: &Rc<VPE>, msg: &'static tcu::Message) -> Result<(), SyscError> {
    let req: &kif::syscalls::Exchange = get_request(msg)?;
    let vpe_sel = req.vpe_sel as CapSel;
    let own_crd = CapRngDesc::new_from(req.own_crd);
    let other_crd = CapRngDesc::new(own_crd.cap_type(), req.other_sel as CapSel, own_crd.count());
    let obtain = req.obtain == 1;

    sysc_log!(
        vpe,
        "exchange(vpe={}, own={}, other={}, obtain={})",
        vpe_sel,
        own_crd,
        other_crd,
        obtain
    );

    let vpecap: Weak<VPE> = get_kobj!(vpe, vpe_sel, VPE);
    let vpecap = vpecap.upgrade().unwrap();

    do_exchange(vpe, &vpecap, &own_crd, &other_crd, obtain)?;

    reply_success(msg);
    Ok(())
}

#[inline(never)]
pub fn exchange_over_sess(
    vpe: &Rc<VPE>,
    msg: &'static tcu::Message,
    obtain: bool,
) -> Result<(), SyscError> {
    let req: &kif::syscalls::ExchangeSess = get_request(msg)?;
    let vpe_sel = req.vpe_sel as CapSel;
    let sess_sel = req.sess_sel as CapSel;
    let crd = CapRngDesc::new_from(req.crd);

    sysc_log!(
        vpe,
        "{}(vpe={}, sess={}, crd={})",
        if obtain { "obtain" } else { "delegate" },
        vpe_sel,
        sess_sel,
        crd
    );

    let vpecap: Weak<VPE> = get_kobj!(vpe, vpe_sel, VPE);
    let vpecap = vpecap.upgrade().unwrap();
    let sess: Rc<SessObject> = get_kobj!(vpe, sess_sel, Sess);

    let smsg = kif::service::Exchange {
        opcode: if obtain {
            kif::service::Operation::OBTAIN.val as u64
        }
        else {
            kif::service::Operation::DELEGATE.val as u64
        },
        sess: sess.ident(),
        data: kif::service::ExchangeData {
            caps: crd.count() as u64,
            args: req.args.clone(),
        },
    };

    let serv: Rc<ServObject> = sess.service().clone();
    let label = sess.creator() as tcu::Label;

    klog!(
        SERV,
        "Sending {}(sess={:#x}, caps={}, args={}B) to service {} with creator {}",
        if obtain { "OBTAIN" } else { "DELEGATE" },
        sess.ident(),
        crd.count(),
        { req.args.bytes },
        serv.service().name(),
        label,
    );
    let rmsg = Service::send_receive(serv.service(), label, util::object_to_bytes(&smsg)).map_err(
        |e| {
            SyscError::new(
                e.code(),
                format!("Service {} unreachable", serv.service().name()),
            )
        },
    )?;

    let reply: &kif::service::ExchangeReply = get_request(rmsg)?;

    sysc_log!(
        vpe,
        "{} continue with res={}",
        if obtain { "obtain" } else { "delegate" },
        { reply.res }
    );

    if reply.res != 0 {
        sysc_err!(Code::from(reply.res as u32), "Server denied cap exchange");
    }
    else {
        do_exchange(
            &vpecap,
            &serv.service().vpe(),
            &crd,
            &CapRngDesc::new_from(reply.data.caps),
            obtain,
        )?;
    }

    let kreply = kif::syscalls::ExchangeSessReply {
        error: 0,
        args: reply.data.args.clone(),
    };
    send_reply(msg, &kreply);

    Ok(())
}

#[inline(never)]
pub fn revoke(vpe: &Rc<VPE>, msg: &'static tcu::Message) -> Result<(), SyscError> {
    let req: &kif::syscalls::Revoke = get_request(msg)?;
    let vpe_sel = req.vpe_sel as CapSel;
    let crd = CapRngDesc::new_from(req.crd);
    let own = req.own == 1;

    sysc_log!(vpe, "revoke(vpe={}, crd={}, own={})", vpe_sel, crd, own);

    if crd.cap_type() == CapType::OBJECT && crd.start() <= kif::SEL_VPE {
        sysc_err!(Code::InvArgs, "Cap 0, 1, and 2 are not revokeable");
    }

    let kobj = match vpe.obj_caps().borrow().get(vpe_sel) {
        Some(c) => c.get().clone(),
        None => sysc_err!(Code::InvArgs, "Invalid capability"),
    };

    if let KObject::VPE(ref v) = kobj {
        let v = v.upgrade().unwrap();
        VPE::revoke(&v, crd, own);
    }
    else {
        sysc_err!(Code::InvArgs, "Invalid capability");
    }

    reply_success(msg);
    Ok(())
}