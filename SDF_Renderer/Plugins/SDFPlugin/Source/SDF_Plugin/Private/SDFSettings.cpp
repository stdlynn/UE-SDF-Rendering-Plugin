#include "SDFSettings.h"

#if WITH_EDITOR
void USDFSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() ==
		 GET_MEMBER_NAME_CHECKED(USDFSettings, Materials))
	{
		MaterialsVersion++;
	}
	else
	{
		SettingsVersion++;
	}
}
#endif