# Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion.

nh_cmps := nh_dft=hs

# Ns
.cf.ns.mlt := 64
.cf.nh.mv_dyn := 0
.cf.nh.pmem := 1
.cf.nh.dmem := 1

.PHONY: prj.tb
prj.tb:
	$(call nm.stg,lib,)
	$(call nm.arc,lib,lib_ns,../std,ns)
	$(call nm.arc,lib,lib_nh,../std,nh)
	$(call nm.arc,lib,lib_us,../std,us)
	$(call nm.arc,lib,lib_cor,lib,tb_cor)
	$(call nm.stg,prc,lib)
	$(call nm.obj,prc,prc_hs,../std,hs)
	$(call nm.fobj,prc,prc_tb,prc,tb)
	$(call nm.ext,prc,prc_als,../std,als) $(nh_cmps) nh_main=prc_tb_main
	$(call nm.ext,prc,prc_pck,../std,pck)
	$(call nm.pck,prc)
	$(call nm.lnk,prc)
