#!/usr/bin/env python3
"""
SimuLTE Null-Check Fix Script
==============================
Tum crash fix'lerini tek seferde uygular.
Daha once uygulanmissa tekrar uygulamaz (guvenli).

Degistirilen dosyalar:
1. LtePhyUe.cc        - cellInfo_ null check
2. LteCellInfo.h      - lambdaUpdate icinde binder_ null check
3. LteCellInfo.cc     - attachUser icinde binder_ null check
4. LteMacUe.cc        - eNB module zincirleme cagri null check

Kullanim:
    python3 apply_all_fixes.py
"""

import os
import sys

BASE = os.path.expanduser("~/my_workspace/test_workspace/simulte/src")

results = []

# =============================================================================
# FIX 1: LtePhyUe.cc - cellInfo_ null check
# =============================================================================
def fix_lte_phy_ue():
    path = os.path.join(BASE, "stack/phy/layer/LtePhyUe.cc")
    with open(path, 'r') as f:
        content = f.read()

    # Zaten uygulanmis mi kontrol et
    if "if (cellInfo_ != nullptr)" in content:
        return "ZATEN UYGULANMIS (atlanıyor)"

    old = (
        '        cellInfo_ = getCellInfo(nodeId_);\n'
        '        int index = intuniform(0, binder_->phyPisaData.maxChannel() - 1);\n'
        '        cellInfo_->lambdaInit(nodeId_, index);\n'
        '        cellInfo_->channelUpdate(nodeId_, intuniform(1, binder_->phyPisaData.maxChannel2()));'
    )

    new = (
        '        cellInfo_ = getCellInfo(nodeId_);\n'
        '        if (cellInfo_ != nullptr)\n'
        '        {\n'
        '            int index = intuniform(0, binder_->phyPisaData.maxChannel() - 1);\n'
        '            cellInfo_->lambdaInit(nodeId_, index);\n'
        '            cellInfo_->channelUpdate(nodeId_, intuniform(1, binder_->phyPisaData.maxChannel2()));\n'
        '        }'
    )

    if old in content:
        content = content.replace(old, new)
        with open(path, 'w') as f:
            f.write(content)
        return "BASARILI"
    else:
        return "HATA: Hedef blok bulunamadi"

# =============================================================================
# FIX 2: LteCellInfo.h - lambdaUpdate icinde binder_ null check
# =============================================================================
def fix_lte_cell_info_h():
    path = os.path.join(BASE, "corenetwork/lteCellInfo/LteCellInfo.h")
    with open(path, 'r') as f:
        content = f.read()

    # Zaten uygulanmis mi
    if "if (binder_ == nullptr) return;" in content:
        return "ZATEN UYGULANMIS (atlanıyor)"

    old = (
        '    void lambdaUpdate(MacNodeId id, unsigned int index)\n'
        '    {\n'
        '        lambdaMap_[id].lambdaMax = binder_->phyPisaData.getLambda(index, 0);'
    )

    new = (
        '    void lambdaUpdate(MacNodeId id, unsigned int index)\n'
        '    {\n'
        '        if (binder_ == nullptr) return;\n'
        '        lambdaMap_[id].lambdaMax = binder_->phyPisaData.getLambda(index, 0);'
    )

    if old in content:
        content = content.replace(old, new)
        with open(path, 'w') as f:
            f.write(content)
        return "BASARILI"
    else:
        return "HATA: Hedef blok bulunamadi"

# =============================================================================
# FIX 3: LteCellInfo.cc - attachUser icinde binder_ null check
# =============================================================================
def fix_lte_cell_info_cc():
    path = os.path.join(BASE, "corenetwork/lteCellInfo/LteCellInfo.cc")
    with open(path, 'r') as f:
        content = f.read()

    # Zaten uygulanmis mi
    if "if (binder_ == nullptr)" in content and "attachUser" in content:
        # Daha kesin kontrol
        idx = content.find("void LteCellInfo::attachUser")
        if idx != -1:
            block = content[idx:idx+300]
            if "if (binder_ == nullptr)" in block:
                return "ZATEN UYGULANMIS (atlanıyor)"

    old = (
        'void LteCellInfo::attachUser(MacNodeId nodeId)\n'
        '{\n'
        '    // add UE to cellInfo\'s structures (lambda maps)\n'
        '    // position will be added by the eNB while computing feedback\n'
        '    int index = intuniform(0, binder_->phyPisaData.maxChannel() - 1);'
    )

    new = (
        'void LteCellInfo::attachUser(MacNodeId nodeId)\n'
        '{\n'
        '    // add UE to cellInfo\'s structures (lambda maps)\n'
        '    // position will be added by the eNB while computing feedback\n'
        '    if (binder_ == nullptr) {\n'
        '        EV << "LteCellInfo::attachUser - WARNING: binder_ is null for nodeId=" << nodeId << endl;\n'
        '        return;\n'
        '    }\n'
        '    int index = intuniform(0, binder_->phyPisaData.maxChannel() - 1);'
    )

    if old in content:
        content = content.replace(old, new)
        with open(path, 'w') as f:
            f.write(content)
        return "BASARILI"
    else:
        return "HATA: Hedef blok bulunamadi"

# =============================================================================
# FIX 4: LteMacUe.cc - eNB module zincirleme cagri null check
# =============================================================================
def fix_lte_mac_ue():
    path = os.path.join(BASE, "stack/mac/layer/LteMacUe.cc")
    with open(path, 'r') as f:
        content = f.read()

    # Zaten uygulanmis mi
    if "LteMacEnb* macEnb = dynamic_cast<LteMacEnb *>(enbMac);" in content:
        return "ZATEN UYGULANMIS (atlanıyor)"

    old = (
        '        LteAmc *amc = check_and_cast<LteMacEnb *>'
        '(getSimulation()->getModule(binder_->getOmnetId(cellId_))'
        '->getSubmodule("lteNic")->getSubmodule("mac"))->getAmc();\n'
        '        amc->attachUser(nodeId_, UL);\n'
        '        amc->attachUser(nodeId_, DL);'
    )

    new = (
        '        int enbOmnetId = binder_->getOmnetId(cellId_);\n'
        '        cModule* enbModule = getSimulation()->getModule(enbOmnetId);\n'
        '        if (enbModule == nullptr) {\n'
        '            EV << "LteMacUe::initialize - WARNING: eNB module not found for cellId=" << cellId_ << ", omnetId=" << enbOmnetId << endl;\n'
        '            return;\n'
        '        }\n'
        '        cModule* enbNic = enbModule->getSubmodule("lteNic");\n'
        '        if (enbNic == nullptr) {\n'
        '            EV << "LteMacUe::initialize - WARNING: lteNic not found in eNB module" << endl;\n'
        '            return;\n'
        '        }\n'
        '        cModule* enbMac = enbNic->getSubmodule("mac");\n'
        '        if (enbMac == nullptr) {\n'
        '            EV << "LteMacUe::initialize - WARNING: mac submodule not found in lteNic" << endl;\n'
        '            return;\n'
        '        }\n'
        '        LteMacEnb* macEnb = dynamic_cast<LteMacEnb *>(enbMac);\n'
        '        if (macEnb == nullptr) {\n'
        '            EV << "LteMacUe::initialize - WARNING: mac module is not LteMacEnb" << endl;\n'
        '            return;\n'
        '        }\n'
        '        LteAmc *amc = macEnb->getAmc();\n'
        '        if (amc == nullptr) {\n'
        '            EV << "LteMacUe::initialize - WARNING: AMC is null" << endl;\n'
        '            return;\n'
        '        }\n'
        '        amc->attachUser(nodeId_, UL);\n'
        '        amc->attachUser(nodeId_, DL);'
    )

    if old in content:
        content = content.replace(old, new)
        with open(path, 'w') as f:
            f.write(content)
        return "BASARILI"
    else:
        # Satir numarasi bazli dene (orijinal dosyada farkli whitespace olabilir)
        with open(path, 'r') as f:
            lines = f.readlines()
        
        target_line = None
        for i, line in enumerate(lines):
            if 'check_and_cast<LteMacEnb' in line:
                target_line = i
                break
        
        if target_line is not None:
            # target_line ve sonraki 2 satiri (attachUser UL, DL) degistir
            # Onceki satir comment satiri ise onu da icerir
            start = target_line
            if start > 0 and 'only for UEs' in lines[start-1]:
                start = start - 1
            
            # attachUser satirlarini bul
            end = target_line + 1
            while end < len(lines) and 'attachUser' in lines[end]:
                end += 1
            
            new_lines = [
                '        // only for UEs that have been added dynamically to the simulation\n',
                '        int enbOmnetId = binder_->getOmnetId(cellId_);\n',
                '        cModule* enbModule = getSimulation()->getModule(enbOmnetId);\n',
                '        if (enbModule == nullptr) {\n',
                '            EV << "LteMacUe::initialize - WARNING: eNB module not found for cellId=" << cellId_ << ", omnetId=" << enbOmnetId << endl;\n',
                '            return;\n',
                '        }\n',
                '        cModule* enbNic = enbModule->getSubmodule("lteNic");\n',
                '        if (enbNic == nullptr) {\n',
                '            EV << "LteMacUe::initialize - WARNING: lteNic not found in eNB module" << endl;\n',
                '            return;\n',
                '        }\n',
                '        cModule* enbMac = enbNic->getSubmodule("mac");\n',
                '        if (enbMac == nullptr) {\n',
                '            EV << "LteMacUe::initialize - WARNING: mac submodule not found in lteNic" << endl;\n',
                '            return;\n',
                '        }\n',
                '        LteMacEnb* macEnb = dynamic_cast<LteMacEnb *>(enbMac);\n',
                '        if (macEnb == nullptr) {\n',
                '            EV << "LteMacUe::initialize - WARNING: mac module is not LteMacEnb" << endl;\n',
                '            return;\n',
                '        }\n',
                '        LteAmc *amc = macEnb->getAmc();\n',
                '        if (amc == nullptr) {\n',
                '            EV << "LteMacUe::initialize - WARNING: AMC is null" << endl;\n',
                '            return;\n',
                '        }\n',
                '        amc->attachUser(nodeId_, UL);\n',
                '        amc->attachUser(nodeId_, DL);\n',
            ]
            
            lines[start:end] = new_lines
            with open(path, 'w') as f:
                f.writelines(lines)
            return "BASARILI (satir bazli)"
        
        return "HATA: Hedef blok bulunamadi"


# =============================================================================
# MAIN
# =============================================================================
print("=" * 60)
print("SimuLTE Null-Check Fix Script")
print("=" * 60)

fixes = [
    ("1. LtePhyUe.cc       (cellInfo_ null check)",      fix_lte_phy_ue),
    ("2. LteCellInfo.h      (lambdaUpdate binder_ check)", fix_lte_cell_info_h),
    ("3. LteCellInfo.cc     (attachUser binder_ check)",   fix_lte_cell_info_cc),
    ("4. LteMacUe.cc        (eNB module chain check)",     fix_lte_mac_ue),
]

all_ok = True
for desc, func in fixes:
    try:
        result = func()
    except FileNotFoundError as e:
        result = f"DOSYA BULUNAMADI: {e}"
        all_ok = False
    except Exception as e:
        result = f"HATA: {e}"
        all_ok = False
    
    if "HATA" in result:
        all_ok = False
    
    print(f"  {desc}  =>  {result}")

print("=" * 60)
if all_ok:
    print("Tum fix'ler basariyla uygulandi veya zaten uygulanmisti.")
    print("Simdi derleyin: cd ~/my_workspace/test_workspace/simulte/src && make -j$(nproc)")
else:
    print("BAZI FIX'LER UYGULANAMADI - yukardaki hatalari kontrol edin.")
print("=" * 60)

