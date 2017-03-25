layerOffset=42
timeWindow=10
sides=2
PEthreshold=12
moduleGap=3

for PEthreshold in {10..24..2}
do
    for photonYield in {2604,2897,3189,3481,3774,4066,4359,4651,4943,5236,5528,5820,6113,6405,6697,6990,7282,7574,7867}  # 20,22,...,56 PE/SiPM 1m away from SiPM for 5cm wide / 3m long counter
    do

              directory=/pnfs/mu2e/scratch/users/ehrlich/workflow
              files=`ls $directory/CRV_Efficiency_check_5cm/outstage/*.CRV_efficiency5cm_top_moduleGap$moduleGap'_'layerOffset$layerOffset'_'photonYield$photonYield/*/*/log.*.log`
#              files=`ls $directory/CRV_Efficiency_check_5cm10/outstage/*.CRV_efficiency5cm10_top_moduleGap$moduleGap'_'layerOffset$layerOffset'_'photonYield$photonYield/*/*/log.*.log`
#              files=`ls $directory/CRV_Efficiency_check_5cm_6600/outstage/*.CRV_efficiency5cm_top6600_moduleGap$moduleGap'_'layerOffset$layerOffset'_'photonYield$photonYield/*/*/log.*.log`
#              files=`ls $directory/CRV_Efficiency_check_5cm10_6600/outstage/*.CRV_efficiency5cm10_top6600_moduleGap$moduleGap'_'layerOffset$layerOffset'_'photonYield$photonYield/*/*/log.*.log`
#              files=`ls $directory/CRV_Efficiency_check_5cm_verticalPlanes/outstage/*.CRV_efficiency5cm_side_moduleGap$moduleGap'_'layerOffset$layerOffset'_'photonYield$photonYield/*/*/log.*.log`
#              files=`ls $directory/CRV_Efficiency_check_5cm_downstreamPlanes/outstage/*.CRV_efficiency5cm_downstream_moduleGap$moduleGap'_'layerOffset$layerOffset'_'photonYield$photonYield/*/*/log.*.log`

              events=0
              eventsCoincidence=0

              for file in $files;
              do

                searchString="SUMMARY CrvCoincidencePE$PEthreshold""T$timeWindow"
#                searchCommand="grep '$searchString' $file"
                searchCommand="tail -n 150 $file | grep '$searchString'"
                searchResult=`eval $searchCommand`
                if [ $? -ne 0 ]; then
                  continue
                fi

                s=($searchResult)
                eventsTmp=${s[4]}
                eventsCoincidenceTmp=${s[2]}

                events=$((events+eventsTmp))
                eventsCoincidence=$((eventsCoincidence+eventsCoincidenceTmp))

              done

              efficiency=`echo "$eventsCoincidence/$events" | bc -l`
              echo modulegap:$moduleGap layerOffset:$layerOffset photonYield:$photonYield PEThreshold:$PEthreshold timeWindow:$timeWindow sidesChecked:$sides eventsCoincidence:$eventsCoincidence eventsTotal:$events efficiency:$efficiency

    done
done