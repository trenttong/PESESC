
class PowerInterface {
 public:
  typedef std::vector<int> ActivityVectorType;

  PowerInterface(const char *confname);
  
  size_t getNumBlocks() const;
  
  // Time between updates is controlled by esesc.conf
  void updateActivity(const ActivityVectorType *vact);

  // Get the temperature for a given sensor id
  float getTemp(int sid) const;

  float getInstPower() const;

  float getInstPower(int sid) const;
};
