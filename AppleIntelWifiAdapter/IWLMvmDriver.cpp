//
//  IWLMvmDriver.cpp
//  AppleIntelWifiAdapter
//
//  Created by 钟先耀 on 2020/1/17.
//  Copyright © 2020 钟先耀. All rights reserved.
//

#include "IWLMvmDriver.hpp"
#include "fw/IWLUcodeParse.hpp"

bool IWLMvmDriver::init(IOPCIDevice *pciDevice)
{
    this->m_pDevice = new IWLDevice();
    if (!this->m_pDevice->init(pciDevice)) {
        return false;
    }
    this->trans = new IWLTransport();
    if (!trans->init(m_pDevice)) {
        return false;
    }
    return true;
}

void IWLMvmDriver::release()
{
    OSSafeReleaseNULL(trans);
    OSSafeReleaseNULL(m_pDevice);
}

bool IWLMvmDriver::probe()
{
    IOInterruptState flags;
    const struct iwl_cfg *cfg_7265d = NULL;
    UInt16 vendorID = m_pDevice->pciDevice->configRead16(kIOPCIConfigVendorID);
    if (vendorID != PCI_VENDOR_ID_INTEL) {
        return false;
    }
    UInt16 deviceID = m_pDevice->deviceID;
    UInt16 subSystemDeviceID = m_pDevice->subSystemDeviceID;
    
    IOLog("hw_rf_id=0x%02x\n", m_pDevice->hw_rf_id);
    IOLog("hw_rev=0x%02x\n", m_pDevice->hw_rev);

    for (int i = 0; i < ARRAY_SIZE(iwl_dev_info_table); i++) {
        const struct iwl_dev_info *dev_info = &iwl_dev_info_table[i];
        if ((dev_info->device == (u16)IWL_CFG_ANY ||
             dev_info->device == deviceID) &&
            (dev_info->subdevice == (u16)IWL_CFG_ANY ||
             dev_info->subdevice == subSystemDeviceID) &&
            (dev_info->mac_type == (u16)IWL_CFG_ANY ||
             dev_info->mac_type ==
             CSR_HW_REV_TYPE(m_pDevice->hw_rev)) &&
            (dev_info->mac_step == (u8)IWL_CFG_ANY ||
             dev_info->mac_step ==
             CSR_HW_REV_STEP(m_pDevice->hw_rev)) &&
            (dev_info->rf_type == (u16)IWL_CFG_ANY ||
             dev_info->rf_type ==
             CSR_HW_RFID_TYPE(m_pDevice->hw_rf_id)) &&
            (dev_info->rf_id == (u8)IWL_CFG_ANY ||
             dev_info->rf_id ==
             IWL_SUBDEVICE_RF_ID(subSystemDeviceID)) &&
            (dev_info->no_160 == (u8)IWL_CFG_ANY ||
             dev_info->no_160 ==
             IWL_SUBDEVICE_NO_160(subSystemDeviceID)) &&
            (dev_info->cores == (u8)IWL_CFG_ANY ||
             dev_info->cores ==
             IWL_SUBDEVICE_CORES(subSystemDeviceID))) {
            m_pDevice->cfg = dev_info->cfg;
            m_pDevice->name = dev_info->name;
        }
    }

//    if (!m_pDevice->cfg) {
//        for (int i = 0; i < ARRAY_SIZE(iwl_hw_card_ids); i++) {
//            pci_device_id dev = iwl_hw_card_ids[i];
//            if (dev.subdevice == subSystemDeviceID && dev.device == deviceID) {
//                m_pDevice->cfg = (struct iwl_cfg *)dev.driver_data;
//                IOLog("我找到啦！！！");
//                break;
//            }
//        }
//    }
    
    if (m_pDevice->cfg) {
        /*
         * special-case 7265D, it has the same PCI IDs.
         *
         * Note that because we already pass the cfg to the transport above,
         * all the parameters that the transport uses must, until that is
         * changed, be identical to the ones in the 7265D configuration.
         */
        if (m_pDevice->cfg == &iwl7265_2ac_cfg)
            cfg_7265d = &iwl7265d_2ac_cfg;
        else if (m_pDevice->cfg == &iwl7265_2n_cfg)
            cfg_7265d = &iwl7265d_2n_cfg;
        else if (m_pDevice->cfg == &iwl7265_n_cfg)
            cfg_7265d = &iwl7265d_n_cfg;
        if (cfg_7265d &&
            (m_pDevice->hw_rev & CSR_HW_REV_TYPE_MSK) == CSR_HW_REV_TYPE_7265D)
            m_pDevice->cfg = cfg_7265d;

        if (m_pDevice->cfg == &iwlax210_2ax_cfg_so_hr_a0) {
            if (m_pDevice->hw_rev == CSR_HW_REV_TYPE_TY) {
                m_pDevice->cfg = &iwlax210_2ax_cfg_ty_gf_a0;
            } else if (CSR_HW_RF_ID_TYPE_CHIP_ID(m_pDevice->hw_rf_id) ==
                       CSR_HW_RF_ID_TYPE_CHIP_ID(CSR_HW_RF_ID_TYPE_JF)) {
                m_pDevice->cfg = &iwlax210_2ax_cfg_so_jf_a0;
            } else if (CSR_HW_RF_ID_TYPE_CHIP_ID(m_pDevice->hw_rf_id) ==
                       CSR_HW_RF_ID_TYPE_CHIP_ID(CSR_HW_RF_ID_TYPE_GF)) {
                m_pDevice->cfg = &iwlax211_2ax_cfg_so_gf_a0;
            } else if (CSR_HW_RF_ID_TYPE_CHIP_ID(m_pDevice->hw_rf_id) ==
                       CSR_HW_RF_ID_TYPE_CHIP_ID(CSR_HW_RF_ID_TYPE_GF4)) {
                m_pDevice->cfg = &iwlax411_2ax_cfg_so_gf4_a0;
            }
        } else if (m_pDevice->cfg == &iwl_ax101_cfg_qu_hr) {
            if ((CSR_HW_RF_ID_TYPE_CHIP_ID(m_pDevice->hw_rf_id) ==
                 CSR_HW_RF_ID_TYPE_CHIP_ID(CSR_HW_RF_ID_TYPE_HR) &&
                 m_pDevice->hw_rev == CSR_HW_REV_TYPE_QNJ_B0) ||
                (CSR_HW_RF_ID_TYPE_CHIP_ID(m_pDevice->hw_rf_id) ==
                 CSR_HW_RF_ID_TYPE_CHIP_ID(CSR_HW_RF_ID_TYPE_HR1))) {
                m_pDevice->cfg = &iwl22000_2ax_cfg_qnj_hr_b0;
            } else if (CSR_HW_RF_ID_TYPE_CHIP_ID(m_pDevice->hw_rf_id) ==
                       CSR_HW_RF_ID_TYPE_CHIP_ID(CSR_HW_RF_ID_TYPE_HR) &&
                       m_pDevice->hw_rev == CSR_HW_REV_TYPE_QUZ) {
                m_pDevice->cfg = &iwl_ax101_cfg_quz_hr;
            } else if (CSR_HW_RF_ID_TYPE_CHIP_ID(m_pDevice->hw_rf_id) ==
                       CSR_HW_RF_ID_TYPE_CHIP_ID(CSR_HW_RF_ID_TYPE_HR)) {
                m_pDevice->cfg = &iwl_ax101_cfg_qu_hr;
            } else if (CSR_HW_RF_ID_TYPE_CHIP_ID(m_pDevice->hw_rf_id) ==
                       CSR_HW_RF_ID_TYPE_CHIP_ID(CSR_HW_RF_ID_TYPE_HRCDB)) {
                IWL_ERR(0, "RF ID HRCDB is not supported\n");
                goto error;
            } else {
                IWL_ERR(0, "Unrecognized RF ID 0x%08x\n",
                        CSR_HW_RF_ID_TYPE_CHIP_ID(m_pDevice->hw_rf_id));
                goto error;
            }
        }

        /*
         * The RF_ID is set to zero in blank OTP so read version to
         * extract the RF_ID.
         */
        if (m_pDevice->cfg->trans.rf_id &&
            !CSR_HW_RFID_TYPE(m_pDevice->hw_rf_id)) {
            IOInterruptState flags;

            if (trans->grabNICAccess(&flags)) {
                u32 val;

                val = trans->iwlReadUmacPRPHNoGrab(WFPM_CTRL_REG);
                val |= ENABLE_WFPM;
                trans->iwlWriteUmacPRPHNoGrab(WFPM_CTRL_REG, val);
                val = trans->iwlReadPRPHNoGrab(SD_REG_VER);

                val &= 0xff00;
                switch (val) {
                    case REG_VER_RF_ID_JF:
                        m_pDevice->hw_rf_id = CSR_HW_RF_ID_TYPE_JF;
                        break;
                        /* TODO: get value for REG_VER_RF_ID_HR */
                    default:
                        m_pDevice->hw_rf_id = CSR_HW_RF_ID_TYPE_HR;
                }
                trans->releaseNICAccess(&flags);
            }
        }

        /*
         * This is a hack to switch from Qu B0 to Qu C0.  We need to
         * do this for all cfgs that use Qu B0, except for those using
         * Jf, which have already been moved to the new table.  The
         * rest must be removed once we convert Qu with Hr as well.
         */
        if (m_pDevice->hw_rev == CSR_HW_REV_TYPE_QU_C0) {
            if (m_pDevice->cfg == &iwl_ax101_cfg_qu_hr)
                m_pDevice->cfg = &iwl_ax101_cfg_qu_c0_hr_b0;
            else if (m_pDevice->cfg == &iwl_ax201_cfg_qu_hr)
                m_pDevice->cfg = &iwl_ax201_cfg_qu_c0_hr_b0;
            else if (m_pDevice->cfg == &killer1650s_2ax_cfg_qu_b0_hr_b0)
                m_pDevice->cfg = &killer1650s_2ax_cfg_qu_c0_hr_b0;
            else if (m_pDevice->cfg == &killer1650i_2ax_cfg_qu_b0_hr_b0)
                m_pDevice->cfg = &killer1650i_2ax_cfg_qu_c0_hr_b0;
        }

        /* same thing for QuZ... */
        if (m_pDevice->hw_rev == CSR_HW_REV_TYPE_QUZ) {
            if (m_pDevice->cfg == &iwl_ax101_cfg_qu_hr)
                m_pDevice->cfg = &iwl_ax101_cfg_quz_hr;
            else if (m_pDevice->cfg == &iwl_ax201_cfg_qu_hr)
                m_pDevice->cfg = &iwl_ax201_cfg_quz_hr;
        }

        /* if we don't have a name yet, copy name from the old cfg */
        if (!m_pDevice->name)
            m_pDevice->name = m_pDevice->cfg->name;


        IOLog("device name=%s\n", m_pDevice->name);

//        if (m_pDevice->trans_cfg->mq_rx_supported) {
//            if (!m_pDevice->cfg->num_rbds) {
//                goto error;
//            }
//            trans_pcie->num_rx_bufs = iwl_trans->cfg->num_rbds;
//        } else {
//            trans_pcie->num_rx_bufs = RX_QUEUE_SIZE;
//        }

        if (m_pDevice->cfg->trans.device_family >= IWL_DEVICE_FAMILY_8000 &&
            trans->grabNICAccess(&flags)) {
            u32 hw_step;
            
            IOLog("HW_STEP_LOCATION_BITS\n");

            hw_step = trans->iwlReadUmacPRPHNoGrab(WFPM_CTRL_REG);
            hw_step |= ENABLE_WFPM;
            trans->iwlWriteUmacPRPHNoGrab(WFPM_CTRL_REG, hw_step);
            hw_step = trans->iwlReadPRPHNoGrab(CNVI_AUX_MISC_CHIP);
            hw_step = (hw_step >> HW_STEP_LOCATION_BITS) & 0xF;
            if (hw_step == 0x3)
                m_pDevice->hw_rev = (m_pDevice->hw_rev & 0xFFFFFFF3) |
                (SILICON_C_STEP << 2);
            trans->releaseNICAccess(&flags);
        }
    }
    return true;
error:
    return false;
}

bool IWLMvmDriver::start()
{
    char tag[8];
    char firmware_name[64];
    if (m_pDevice->cfg->trans.device_family == IWL_DEVICE_FAMILY_9000 &&
        (CSR_HW_REV_STEP(m_pDevice->hw_rev) != SILICON_B_STEP &&
         CSR_HW_REV_STEP(m_pDevice->hw_rev) != SILICON_C_STEP)) {
        IWL_ERR(0,
            "Only HW steps B and C are currently supported (0x%0x)\n",
            m_pDevice->hw_rev);
        return false;
    }
    snprintf(tag, sizeof(tag), "%d", m_pDevice->cfg->ucode_api_max);
    snprintf(firmware_name, sizeof(firmware_name), "%s%s.ucode", m_pDevice->cfg->fw_name_pre, tag);
    IWL_INFO(0, "attempting to load firmware '%s'\n", firmware_name);
    IOReturn ret = OSKextRequestResource(OSKextGetCurrentIdentifier(), firmware_name, reqFWCallback, this, NULL);
    if (ret != kIOReturnSuccess) {
        IWL_ERR(0, "Error loading firmware %s\n", firmware_name);
        return false;
    }
    return true;
}

void IWLMvmDriver::reqFWCallback(OSKextRequestTag requestTag, OSReturn result, const void *resourceData, uint32_t resourceDataLength, void *context)
{
    IWL_INFO(0, "request firmware callback\n");
    if (resourceDataLength <= 4) {
        IWL_ERR(drv, "Error loading fw, size=%d\n",resourceDataLength);
        return;
    }
    bool ret = IWLUcodeParse::parseFW(resourceData);
    if (!ret) {
        IWL_ERR(0, "parse fw failed\n");
        return;
    }
    
}

