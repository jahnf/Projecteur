# Fallback for version generation from pure git archive exports

set(GIT_EXPORT_VERSION_SHORTHASH "$Format:%h$")
set(GIT_EXPORT_VERSION_FULLHASH "$Format:%H$")
set(GIT_EXPORT_VERSION_BRANCH "$Format:%D$") # needs parsing in cmake...
set(GIT_EXPORT_VERSION_DATE_MONTH_YEAR "$Format:%cs$")
set(HAS_GIT_EXPORT_INFO 1)
