/**
 * The output mapper builds a relation between output groups (or a collection of
 * groups, called an ubergroup) and an effect routine.
 */
#ifndef OUTPUTMAPPER_H
#define OUTPUTMAPPER_H

#include "HSIPixel.h"
#include "db/Group.h"

#include <map>
#include <set>
#include <vector>
#include <mutex>
#include <exception>
#include <memory>

#include "INIReader.h"

class Routine;

class DataStore;
class Framebuffer;

class OutputMapper {
	friend class EffectRunner;

	public:
		struct invalid_ubergroup : public std::exception {
			const char *what() const throw () {
		    	return "Invalid ubergroup";
		    }
		};

	public:
		class OutputGroup {
			friend class OutputMapper;

			public:
				OutputGroup() = delete;
				OutputGroup(DbGroup *g);
				virtual ~OutputGroup();

        /**
         * Returns the group ID.
         */
        int getGroupId() const {
          return this->group->getId();
        }

				/**
				 * Returns an iterator into the group's framebuffer.
				 */
				HSIPixel *getDataPointer() {
					return this->buffer;
				}

        /**
         * Sets the brightness of this group.
         */
        virtual void setBrightness(double brightness) {
          // bounds checking: [0, 1]
          if(brightness >= 0.0 && brightness <= 1.0) {
            this->brightness = brightness;
          }
        }
        /**
         * Returns the brightness of the group.
         */
        double getBrightness() const {
          return this->brightness;
        }

				virtual int numPixels();

				virtual void bindBufferToRoutine(Routine *r);

        virtual void copyIntoFramebuffer(std::shared_ptr<Framebuffer> fb,
                                         HSIPixel *buffer = nullptr);

			private:
				virtual void _resizeBuffer();

				bool bufferChanged = false;
				Routine *bufferBoundRoutine = nullptr;

				HSIPixel *buffer = nullptr;
				size_t bufferSz = 0;

        /// brightness to scale each output pixel by
        double brightness = 1.0;

			private:
				DbGroup *group = nullptr;

				friend bool operator==(const OutputGroup& lhs, const OutputGroup& rhs);
				friend bool operator< (const OutputGroup& lhs, const OutputGroup& rhs);

				friend std::ostream &operator<<(std::ostream& strm, const OutputGroup& obj);
		};

		class OutputUberGroup: public OutputGroup {
			friend class OutputMapper;

			public:
				OutputUberGroup();
				OutputUberGroup(std::vector<OutputGroup *> &members);
				~OutputUberGroup();

			public:
				// overrides from OutputGroup
				virtual int numPixels();

        virtual void copyIntoFramebuffer(std::shared_ptr<Framebuffer> fb,
                                         HSIPixel *buffer = nullptr);

				int numMembers() {
					return this->groups.size();
				}

			private:
				// virtual void _resizeBuffer();

				void addMember(OutputGroup *group);
				void removeMember(OutputGroup *group);
				bool containsMember(OutputGroup *group);

			private:
				// std::recursive_mutex groupsLock;

				std::set<OutputGroup *> groups;


			// operators
				friend bool operator==(const OutputUberGroup& lhs, const OutputUberGroup& rhs);
				friend bool operator< (const OutputUberGroup& lhs, const OutputUberGroup& rhs);

				friend std::ostream &operator<<(std::ostream& strm, const OutputUberGroup& obj);
		};

	public:
    OutputMapper(std::shared_ptr<DataStore>, std::shared_ptr<Framebuffer>,
                 std::shared_ptr<INIReader>);
		~OutputMapper();

	public:
		void addMapping(OutputGroup *g, Routine *r);
		void removeMappingForGroup(OutputGroup *g);

		inline Routine *routineForMapping(OutputGroup *g) {
			return this->outputMap[g];
		}

    void getAllGroups(std::vector<OutputGroup *> &groups);

	private:
		void _removeMappingsInUbergroup(OutputUberGroup *ug);

		void removeEmptyUbergroups(void);

		void printMap(void);

	private:
    std::shared_ptr<DataStore> store;
    std::shared_ptr<INIReader> config;
    std::shared_ptr<Framebuffer> fb;

		// TODO: indicate if the output config was changed

		std::recursive_mutex outputMapLock;
		std::map<OutputGroup *, Routine *> outputMap;
};

// operators
inline std::ostream &operator<<(std::ostream& strm, const OutputMapper::OutputGroup *obj) {
	strm << *obj;
	return strm;
}
inline std::ostream &operator<<(std::ostream& strm, const OutputMapper::OutputUberGroup *obj) {
	strm << *obj;
	return strm;
}

#endif
